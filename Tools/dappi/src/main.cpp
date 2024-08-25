/*
 * Copyright (c) 2024 Flokart World, Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *    1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 *    2. Altered source versions must be plainly marked as such, and must not
 *    be misrepresented as being the original software.
 *
 *    3. This notice may not be removed or altered from any source
 *    distribution.
 */

#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>
#include <semver.hpp>
#include <minisat/core/Solver.h>
#include "general_violation_counters.hpp"

namespace {

struct required_dependency {
    std::string name;
    std::string require;
    std::vector<std::string> locations;
};

struct integrity_t {
    std::string algorithm;
    std::string digest;
};

struct locked_package {
    semver::version version;
    std::string location;
    integrity_t integrity;
    std::set<std::string> dependencies;
};

struct dap {
    Minisat::Var var;
    semver::version version;
    std::string location;
    std::optional<integrity_t> integrity;
    std::set<std::string> dependencies;
};

using dap_map_t = std::unordered_map<std::string, dap>;

struct named_dap {
    Minisat::Var var;
    dap_map_t::iterator referenced_dap;
};

struct name {
    dap_map_t::iterator selection;
    std::vector<named_dap> candidates;
};

std::pair<required_dependency, bool> parse_dependency(
    const YAML::Node &name,
    const YAML::Node &value,
    bool strict
) {
    required_dependency result;
    bool well_formed = true;
    if (value.Type() == YAML::NodeType::Map) {
        result.name = name.as<std::string>();

        auto require_node = value["require"];
        if (require_node) {
            switch (require_node.Type()) {
            case YAML::NodeType::Scalar:
                result.require = require_node.as<std::string>();
                break;
            default:
                if (strict) {
                    std::cerr << "ERROR: require is not a scalar."
                              << std::endl;
                }
                well_formed = false;
                break;
            }
        }

        auto location_node = value["location"];
        if (location_node) {
            switch (location_node.Type()) {
            case YAML::NodeType::Scalar:
                result.locations = { location_node.as<std::string>() };
                break;
            case YAML::NodeType::Sequence:
                result.locations.reserve(location_node.size());
                for (auto elem : location_node) {
                    if (elem.Type() == YAML::NodeType::Scalar) {
                        result.locations.push_back(elem.as<std::string>());
                    } else {
                        if (strict) {
                            std::cerr << "ERROR: A location is not a scalar."
                                      << std::endl;
                        }
                        well_formed = false;
                    }
                }
                break;
            default:
                if (strict) {
                    std::cerr << "ERROR: location is invalid." << std::endl;
                }
                well_formed = false;
                break;
            }
        }
    } else {
        if (strict) {
            std::cerr << "ERROR: A dependency is not a map." << std::endl;
        }
        well_formed = false;
    }
    return std::make_pair(std::move(result), well_formed);
}

int load_da(const char *filename, bool strict) {
    YAML::Node doc;
    try {
        doc = YAML::LoadFile(filename);
    } catch (std::exception &) {
        std::cerr << "ERROR: Failed to read YAML from " << filename
                  << std::endl;
        return 1;
    }

    std::string name;
    std::string version;
    std::list<required_dependency> dependencies;

    bool well_formed = true;
    if (doc.Type() == YAML::NodeType::Map) {
        auto name_node = doc["name"];
        if (name_node) {
            switch (name_node.Type()) {
            case YAML::NodeType::Scalar:
                name = name_node.as<std::string>();
                break;
            default:
                if (strict) {
                    std::cerr << "ERROR: name is not a scalar." << std::endl;
                }
                well_formed = false;
                break;
            }
        }

        auto version_node = doc["version"];
        if (version_node) {
            switch (version_node.Type()) {
            case YAML::NodeType::Scalar:
                version = version_node.as<std::string>();
                break;
            default:
                if (strict) {
                    std::cerr << "ERROR: version is not a scalar."
                              << std::endl;
                }
                well_formed = false;
                break;
            }
        }

        auto deps_node = doc["dependencies"];
        if (deps_node) {
            switch (deps_node.Type()) {
            case YAML::NodeType::Map:
                for (auto dep_node : deps_node) {
                    auto [new_dep, valid] = parse_dependency(
                        dep_node.first,
                        dep_node.second,
                        strict
                    );
                    if (valid) {
                        dependencies.push_back(std::move(new_dep));
                    } else {
                        well_formed = false;
                    }
                }
                break;
            default:
                if (strict) {
                    std::cerr << "ERROR: dependencies is not a map."
                              << std::endl;
                }
                well_formed = false;
                break;
            }
        }
    } else {
        if (strict) {
            std::cerr << "ERROR: The document is not a map." << std::endl;
        }
        well_formed = false;
    }

    if (well_formed) {

        /* TODO : Escape CMake strings */

        std::cout << "DAP_INFO(" << std::endl;
        if (!name.empty()) {
            std::cout << "  NAME " << name << std::endl;
        }
        if (!version.empty()) {
            std::cout << "  VERSION " << version << std::endl;
        }
        std::cout << ")" << std::endl;

        for (auto dep : dependencies) {
            std::cout << "DAP(" << std::endl
                      << "  NAME " << dep.name << std::endl;
            if (!dep.require.empty()) {
                std::cout << "  REQUIRE \"" << dep.require << "\""
                          << std::endl;
            }
            if (!dep.locations.empty()) {
                std::cout << "  LOCATION" << std::endl;
                for (auto location : dep.locations) {
                    std::cout << "    \"" << location << "\"" << std::endl;
                }
            }
            std::cout << ")" << std::endl;
        }

        return 0;
    } else if (strict) {
        return 1;
    } else {
        /* This revision is to be skipped. */
        return 0;
    }
}

int load_dal(const char *filename, bool /* strict */) {
    YAML::Node doc;
    try {
        doc = YAML::LoadFile(filename);
    } catch (std::exception &) {
        std::cerr << "ERROR: Failed to read YAML from " << filename
                  << std::endl;
        return 1;
    }

    if (doc.Type() != YAML::NodeType::Map) {
        std::cerr << "ERROR: The document is not a map." << std::endl;
        return 1;
    }

    if (auto version_node = doc["version"]; version_node) {
        switch (version_node.Type()) {
        case YAML::NodeType::Scalar:
            if (int version = version_node.as<int>(); version != 1) {
                std::cerr << "ERROR: Unknown version - " << version
                          << std::endl;
                return 1;
            }
            break;
        default:
            std::cerr << "ERROR: version is not a scalar." << std::endl;
            return 1;
        }
    } else {
        std::cerr << "ERROR: version does not exist." << std::endl;
        return 1;
    }

    auto packages_node = doc["packages"];
    if (!packages_node) {
        std::cerr << "ERROR: packages does not exist." << std::endl;
        return 1;
    } else if (packages_node.Type() != YAML::NodeType::Map) {
        std::cerr << "ERROR: packages is not a map." << std::endl;
        return 1;
    }

    std::map<std::string, locked_package> locked_packages;
    for (auto package : packages_node) {
        auto name = package.first.as<std::string>();
        locked_package new_package;

        auto &body = package.second;
        if (body.Type() != YAML::NodeType::Map) {
            std::cerr << "ERROR: package " << name << " is not a map."
                      << std::endl;
            return 1;
        }

        if (auto version_node = body["version"]; version_node) {
            if (version_node.Type() == YAML::NodeType::Scalar) {
                new_package.version = semver::version(
                    version_node.as<std::string>()
                );
            } else {
                std::cerr << "ERROR: version of package " << name
                          << " is not a scalar." << std::endl;
                return 1;
            }
        } else {
            std::cerr << "ERROR: version of package " << name
                      << " does not exist." << std::endl;
            return 1;
        }

        if (auto location_node = body["location"]; location_node) {
            if (location_node.Type() == YAML::NodeType::Scalar) {
                new_package.location = location_node.as<std::string>();
            } else {
                std::cerr << "ERROR: location of package " << name
                          << " is not a scalar." << std::endl;
                return 1;
            }
        } else {
            std::cerr << "ERROR: location of package " << name
                      << " does not exist." << std::endl;
            return 1;
        }

        if (auto integrity_node = body["integrity"]; integrity_node) {
            if (integrity_node.Type() != YAML::NodeType::Map) {
                std::cerr << "ERROR: integrity of package " << name
                          << " is not a map." << std::endl;
                return 1;
            }

            if (auto node = integrity_node["algorithm"]; node) {
                if (node.Type() != YAML::NodeType::Scalar) {
                    std::cerr << "ERROR: integrity algorithm of package "
                              << name << " is not a scalar." << std::endl;
                    return 1;
                }
                new_package.integrity.algorithm = node.as<std::string>();
            } else {
                std::cerr << "ERROR: integrity algorithm of package " << name
                          << " does not exist." << std::endl;
                return 1;
            }

            if (auto node = integrity_node["digest"]; node) {
                if (node.Type() != YAML::NodeType::Scalar) {
                    std::cerr << "ERROR: integrity digest of package "
                              << name << " is not a scalar." << std::endl;
                    return 1;
                }
                new_package.integrity.digest = node.as<std::string>();
            } else {
                std::cerr << "ERROR: integrity digest of package " << name
                          << " does not exist." << std::endl;
                return 1;
            }
        } else {
            std::cerr << "ERROR: integrity of package " << name
                      << " does not exist." << std::endl;
            return 1;
        }

        if (auto deps_node = body["dependencies"]; deps_node) {
            if (deps_node.Type() != YAML::NodeType::Sequence) {
                std::cerr << "ERROR: dependencies of package " << name
                          << " is not a sequence." << std::endl;
                return 1;
            }
            for (auto dep : deps_node) {
                if (dep.Type() != YAML::NodeType::Scalar) {
                    std::cerr << "ERROR: a dependency from package " << name
                              << " is not a scalar." << std::endl;
                    return 1;
                }
                new_package.dependencies.insert(dep.as<std::string>());
            }
        }

        locked_packages.emplace(name, std::move(new_package));
    }

    for (auto &[name, body] : locked_packages) {

        /* TODO : Escape CMake strings */

        std::cout << "DAPPI_LOCK(" << std::endl
                  << "  NAME " << name << std::endl
                  << "  VERSION " << body.version.to_string() << std::endl
                  << "  LOCATION \"" << body.location << "\"" << std::endl
                  << "  INTEGRITY" << std::endl
                  << "    ALGORITHM " << body.integrity.algorithm << std::endl
                  << "    DIGEST " << body.integrity.digest << std::endl
                  << ")" << std::endl;
    }

    return 0;
}

int load(int argc, char *argv[]) {
    bool strict = false;
    const char *input = nullptr;
    int (*loader)(const char *, bool) = nullptr;

    int pos = 0;
    while (pos < argc) {
        std::string_view arg = argv[pos++];
        if (arg == "-t") {
            if (loader) {
                std::cerr << "ERROR: More than one -t are specified."
                          << std::endl;
                return 1;
            } else if (pos == argc) {
                std::cerr << "ERROR: -t requires subsequent argument."
                          << std::endl;
                return 1;
            } else {
                std::string_view type_str = argv[pos++];
                if (type_str == "da") {
                    loader = load_da;
                } else if (type_str == "dal") {
                    loader = load_dal;
                } else {
                    std::cerr << "ERROR: Unknown type - " << type_str
                              << std::endl;
                    return 1;
                }
            }
        } else if (arg == "-i") {
            if (input) {
                std::cerr << "ERROR: More than one -i are specified."
                          << std::endl;
                return 1;
            } else if (pos == argc) {
                std::cerr << "ERROR: -i requires subsequent argument."
                          << std::endl;
                return 1;
            } else {
                input = argv[pos++];
            }
        } else if (arg == "--strict") {
            strict = true;
        } else {
            std::cerr << "ERROR: Unrecognized argument - " << arg << std::endl;
            return 1;
        }
    }

    if (!input) {
        std::cerr << "ERROR: -i option is mandatory." << std::endl;
        return 1;
    }

    if (loader) {
        return loader(input, strict);
    } else {
        std::cerr << "ERROR: -t option is mandatory." << std::endl;
        return 1;
    }
}

int save(int argc, char *argv[]) {
    const char *output = nullptr;

    int pos = 0;
    while (pos < argc) {
        std::string_view arg = argv[pos++];
        if (arg == "-o") {
            if (output) {
                std::cerr << "ERROR: More than one -o are specified."
                          << std::endl;
                return 1;
            } else if (pos == argc) {
                std::cerr << "ERROR: -o requires subsequent argument."
                          << std::endl;
                return 1;
            } else {
                output = argv[pos++];
            }
        } else {
            std::cerr << "ERROR: Unrecognized argument - " << arg << std::endl;
            return 1;
        }
    }

    if (!output) {
        std::cerr << "ERROR: -o option is mandatory." << std::endl;
        return 1;
    }

    nlohmann::json state;
    try {
        std::cin >> state;
    } catch (std::exception &) {
        std::cerr << "ERROR: Failed to read JSON from stdin." << std::endl;
        return 1;
    }

    dap_map_t daps;
    std::map<std::string, dap_map_t::iterator> names;

    auto daps_it = state.find("daps");
    if (daps_it != state.end()) {
        for (auto &[key, value] : daps_it->items()) {
            dap new_dap;

            auto version_it = value.find("version");
            if (version_it != value.end()) {
                new_dap.version = semver::version(
                    version_it->template get<std::string>()
                );
            }

            auto location_it = value.find("location");
            if (location_it != value.end()) {
                new_dap.location = location_it->template get<std::string>();
            }

            auto integrity_it = value.find("integrity");
            if (integrity_it != value.end()) {
                integrity_t integrity;

                auto algorithm_it = integrity_it->find("algorithm");
                if (algorithm_it != integrity_it->end()) {
                    integrity.algorithm =
                            algorithm_it->template get<std::string>();
                }

                auto digest_it = integrity_it->find("digest");
                if (digest_it != integrity_it->end()) {
                    integrity.digest =
                            digest_it->template get<std::string>();
                }

                new_dap.integrity = std::move(integrity);
            }

            auto deps_it = value.find("dependencies");
            if (deps_it != value.end()) {
                for (auto &dep : *deps_it) {
                    auto name_it = dep.find("name");
                    if (name_it != dep.end()) {
                        new_dap.dependencies.insert(
                            name_it->template get<std::string>()
                        );
                    }
                }
            }

            daps.emplace(key, std::move(new_dap));
        }
    }

    auto names_it = state.find("names");
    if (names_it != state.end()) {
        for (auto &[key, value] : names_it->items()) {
            auto selected_it = value.find("selected");
            if (selected_it != value.end()) {
                auto id = selected_it->template get<std::string>();
                auto found_dap = daps.find(id);
                if (found_dap == daps.end()) {
                    std::cerr << "ERROR: DAP " << id << " not defined."
                              << std::endl;
                    return 1;
                } else {
                    names.emplace(key, found_dap);
                }
            }
        }
    }

    YAML::Emitter lockfile;
    lockfile << YAML::DoubleQuoted
             << YAML::BeginMap
             << YAML::Key << "version"
             << YAML::Value << 1
             << YAML::Key << "packages"
             << YAML::Value << YAML::BeginMap;

    for (auto &[name, dap_it] : names) {
        auto &referenced_dap = dap_it->second;
        if (!referenced_dap.integrity) {
            std::cerr << "ERROR: Integrity for DAP " << dap_it->first
                      << " is blank." << std::endl;
            return 1;
        }
        lockfile << YAML::Key << name
                 << YAML::Value << YAML::BeginMap
                 << YAML::Key << "version"
                 << YAML::Value << referenced_dap.version.to_string()
                 << YAML::Key << "location"
                 << YAML::Value << referenced_dap.location
                 << YAML::Key << "integrity"
                 << YAML::Value << YAML::BeginMap
                 << YAML::Key << "algorithm"
                 << YAML::Value << referenced_dap.integrity->algorithm
                 << YAML::Key << "digest"
                 << YAML::Value << referenced_dap.integrity->digest
                 << YAML::EndMap;
        if (!referenced_dap.dependencies.empty()) {
            lockfile << YAML::Key << "dependencies"
                     << YAML::Value << YAML::BeginSeq;
            for (auto &name : referenced_dap.dependencies) {
                lockfile << name;
            }
            lockfile << YAML::EndSeq;
        }
        lockfile << YAML::EndMap;
    }

    lockfile << YAML::EndMap << YAML::EndMap << YAML::Newline;

    std::string output_body = lockfile.c_str();

    std::filebuf output_file;
    output_file.open(output, std::ios::in | std::ios::binary);
    if (output_file.is_open()) {
        auto size = output_file.pubseekoff(0, std::ios::end);
        if (size == output_body.length()) {
            std::string original;
            original.resize(output_body.length());
            output_file.pubseekpos(0);
            if (
                output_file.sgetn(original.data(), output_body.length())
                == output_body.length()
                && original == output_body
            ) {
                /* The lockfile is identical. */
                return 0;
            }
        }
        output_file.close();
    }

    output_file.open(output, std::ios::out | std::ios::binary);
    if (!output_file.is_open()) {
        std::cerr << "ERROR: Failed to open " << output << " as output."
                  << std::endl;
        return 1;
    }
    output_file.sputn(output_body.data(), output_body.length());

    return 0;
}

int run(int, char *[]) {
    nlohmann::json state;
    try {
        std::cin >> state;
    } catch (std::exception &) {
        std::cerr << "ERROR: Failed to read JSON from stdin." << std::endl;
        return 1;
    }

    Minisat::Solver resolution;
    dap_map_t daps;
    std::unordered_map<std::string, name> names;

    auto daps_it = state.find("daps");
    if (daps_it != state.end()) {
        for (auto &[key, value] : daps_it->items()) {
            dap new_dap;
            new_dap.var = resolution.newVar();

            auto version_it = value.find("version");
            if (version_it != value.end()) {
                new_dap.version = semver::version(
                    version_it->template get<std::string>()
                );
            }

            daps.emplace(key, std::move(new_dap));
        }
    }

    std::vector<Minisat::Var> penalties;
    std::vector<Minisat::Var> unlocks;

    auto names_it = state.find("names");
    if (names_it != state.end()) {
        for (auto &[key, value] : names_it->items()) {
            name new_name;
            new_name.selection = daps.end();

            auto selected_it = value.find("selected");
            if (selected_it != value.end()) {
                auto id = selected_it->template get<std::string>();
                auto found_dap = daps.find(id);
                if (found_dap == daps.end()) {
                    std::cerr << "ERROR: DAP " << id << " not defined."
                              << std::endl;
                    return 1;
                } else {
                    new_name.selection = found_dap;
                }
            }

            bool maybe_unlocked = false;
            Minisat::Var unlock = Minisat::var_Undef;
            dap_map_t::iterator locked_dap = daps.end();
            if (auto it = value.find("locked"); it != value.end()) {
                locked_dap = daps.find(it->template get<std::string>());
                if (locked_dap != daps.end()) {
                    unlock = resolution.newVar();
                }
            }

            auto known_it = value.find("known");
            if (known_it != value.end()) {
                auto &candidates = new_name.candidates;

                std::multimap<
                    semver::version,
                    std::list<named_dap *>
                > version_groups;

                candidates.reserve(known_it->size());
                for (auto &id_json : *known_it) {
                    auto id = id_json.template get<std::string>();
                    auto found_dap = daps.find(id);
                    if (found_dap == daps.end()) {
                        std::cerr << "ERROR: DAP " << id << " not defined."
                                  << std::endl;
                        return 1;
                    } else {
                        // Named DAP requires actual DAP instance.
                        named_dap new_named_dap;
                        new_named_dap.var = resolution.newVar();
                        new_named_dap.referenced_dap = found_dap;
                        resolution.addClause(
                            ~Minisat::mkLit(new_named_dap.var),
                            Minisat::mkLit(found_dap->second.var)
                        );

                        /*
                         * Any selection other than the locked package is
                         * counted as an unlock.
                         */
                        if (
                            unlock != Minisat::var_Undef
                            && found_dap != locked_dap
                        ) {
                            resolution.addClause(
                                ~Minisat::mkLit(new_named_dap.var),
                                Minisat::mkLit(unlock)
                            );
                            maybe_unlocked = true;
                        }

                        candidates.push_back(std::move(new_named_dap));

                        auto &ver = found_dap->second.version;
                        auto found = version_groups.find(ver);
                        if (found == version_groups.end()) {
                            found = version_groups.emplace(
                                ver,
                                std::list<named_dap *>()
                            );
                        }
                        found->second.push_back(&candidates.back());
                    }
                }

                // All named DAPs with same name are exclusive.
                std::size_t num_candidates = candidates.size();
                for (auto lhs = 0; lhs < num_candidates; ++lhs) {
                    for (auto rhs = lhs + 1; rhs < num_candidates; ++rhs) {
                        resolution.addClause(
                            ~Minisat::mkLit(candidates[lhs].var),
                            ~Minisat::mkLit(candidates[rhs].var)
                        );
                    }
                }

                /*
                 * !version_penalty[n] implies !version_n_or_less_selected[n]
                 *
                 * Not selecting the package is best. Selecting the latest
                 * version is second best, so we penalize one point.
                 * Selecting earlier versions is worse than that, so we
                 * penalize one point each time it is downgraded.
                 */
                for (
                    auto upper_bound = version_groups.begin();
                    upper_bound != version_groups.end();
                    ++upper_bound
                ) {
                    /*
                     * Given that version_n_or_less_selected[n] is:
                     *   A or B or C,
                     * !penalty[n] implies !(A or B or C).
                     * (A or B or C) implies penalty[n] is identical to
                     * !(A or B or C) or penalty[n], which is identical to
                     * (!A and !B and !C) or penalty[n].
                     */
                    Minisat::Var penalty = resolution.newVar();
                    auto end = std::next(upper_bound);
                    for (auto it = version_groups.begin(); it != end; ++it) {
                        for (auto candidate : it->second) {
                            resolution.addClause(
                                ~Minisat::mkLit(candidate->var),
                                Minisat::mkLit(penalty)
                            );
                        }
                    }
                    penalties.push_back(penalty);
                }
            }

            if (maybe_unlocked) {
                unlocks.push_back(unlock);
            }

            names.emplace(key, std::move(new_name));
        }
    }

    if (daps_it != state.end()) {
        for (auto &[key, value] : daps_it->items()) {
            auto &this_dap = daps[key];
            auto deps_it = value.find("dependencies");
            if (deps_it != value.end()) {
                for (auto &dep : *deps_it) {
                    auto name_it = dep.find("name");
                    if (name_it == dep.end()) {
                        std::cerr << "ERROR: Invalid dependency." << std::endl;
                        return 1;
                    }
                    std::string req = "*";
                    auto req_it = dep.find("requiredVersion");
                    if (req_it != dep.end()) {
                        req = req_it->template get<std::string>();
                    }
                    auto name_str = name_it->template get<std::string>();
                    auto found_name = names.find(name_str);
                    if (found_name == names.end()) {
                        std::cerr << "ERROR: name " << name_str << " not found"
                                  << std::endl;
                        return 1;
                    }
                    auto &candidates = found_name->second.candidates;
                    auto new_var = resolution.newVar();
                    resolution.addClause(
                        ~Minisat::mkLit(this_dap.var),
                        Minisat::mkLit(new_var)
                    );
                    Minisat::vec<Minisat::Lit> clause;
                    clause.push(~Minisat::mkLit(new_var));
                    for (auto &candidate : candidates) {
                        auto &ver = candidate.referenced_dap->second.version;
                        if (
                            satisfies(
                                ver,
                                req,
                                semver::range
                                      ::satisfies_option
                                      ::include_prerelease
                            )
                        ) {
                            clause.push(Minisat::mkLit(candidate.var));
                        }
                    }
                    if (clause.size() > 1) {
                        resolution.addClause(clause);
                    } else {
                        std::cerr << "ERROR: No matching versions for package "
                                  << name_str << " version " << req
                                  << std::endl;
                        return 1;
                    }
                }
            }
        }
    }

    auto entry_it = state.find("entry");
    if (entry_it != state.end()) {
        auto id = entry_it->template get<std::string>();
        auto found_dap = daps.find(id);
        if (found_dap == daps.end()) {
            std::cerr << "ERROR: DAP " << id << " not defined." << std::endl;
            return 1;
        } else {
            resolution.addClause(Minisat::mkLit(found_dap->second.var));
        }
    }

    auto save_selections = [&]() {
        for (auto &name_pair : names) {
            auto selected_dap = daps.end();
            for (auto &candidate : name_pair.second.candidates) {
                if (resolution.modelValue(candidate.var) == Minisat::l_True) {
                    selected_dap = candidate.referenced_dap;
                    break;
                }
            }
            name_pair.second.selection = selected_dap;
        }
    };

    if (!resolution.solve()) {
        std::cerr << "ERROR: Dependency conflicted." << std::endl;
        return 1;
    }

    save_selections();

    if (!unlocks.empty()) {
        /* Now we minimize unlocks. */
        auto unlock_counters =
                make_general_violation_counters(resolution, unlocks);
        auto last_assumption = Minisat::lit_Undef;
        auto satisfiable = [&](std::nullptr_t, Minisat::Var var) {
            auto assumption = ~Minisat::mkLit(var);
            if (resolution.solve(assumption)) {
                last_assumption = assumption;
                save_selections();
                return true;
            } else {
                return false;
            }
        };
        std::ignore = std::upper_bound(
            unlock_counters.begin(),
            unlock_counters.end(),
            nullptr,
            satisfiable
        );
        if (last_assumption != Minisat::lit_Undef) {
            resolution.addClause(last_assumption);
        }
    }

    if (!penalties.empty()) {
        /* Now we improve the model */
        auto penalty_counters =
                make_general_violation_counters(resolution, penalties);
        auto satisfiable = [&](std::nullptr_t, Minisat::Var var) {
            auto assumption = ~Minisat::mkLit(var);
            if (resolution.solve(assumption)) {
                save_selections();
                return true;
            } else {
                return false;
            }
        };
        std::ignore = std::upper_bound(
            penalty_counters.begin(),
            penalty_counters.end(),
            nullptr,
            satisfiable
        );
    }

    for (auto &name_pair : names) {
        auto selected_dap = name_pair.second.selection;
        if (selected_dap == daps.end()) {
            std::cout << "DAPPI_UNSELECT(" << name_pair.first << ")"
                      << std::endl;
        } else {
            std::cout << "DAPPI_SELECT("
                      << name_pair.first
                      << " "
                      << selected_dap->first
                      << ")"
                      << std::endl;
        }
    }

    return 0;
}

} // namespace

int main(int argc, char *argv[]) {
    int pos = 1;
    int (*subcommand)(int, char *[]) = nullptr;

    if (pos < argc) {
        std::string_view arg = argv[pos++];
        if (arg == "load") {
            subcommand = load;
        } else if (arg == "save") {
            subcommand = save;
        } else if (arg == "run") {
            subcommand = run;
        } else {
            std::cerr << "ERROR: Unrecognized argument - " << arg << std::endl;
            return 1;
        }
    }

    if (subcommand) {
        return subcommand(argc - pos, argv + pos);
    } else {
        std::cerr << "ERROR: At least one argument is required." << std::endl;
        return 1;
    }
}
