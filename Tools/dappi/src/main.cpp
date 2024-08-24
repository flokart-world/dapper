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

#include <list>
#include <map>
#include <iostream>
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

struct dap {
    Minisat::Var var;
    semver::version version;
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

int load_dal(const char * /* filename */, bool /* strict */) {

    /* TODO */

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

int save(int /* argc */, char * /* argv */[]) {

    /* TODO */

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

    if (!penalties.empty()) {
        auto penalty_counters =
                make_general_violation_counters(resolution, penalties);

        /*
         * Now we improve the model by adding assumption.
         */
        for (std::size_t num = penalties.size() - 1; num > 0; --num) {
            auto assumption = ~Minisat::mkLit(penalty_counters.at_least(num));
            if (!resolution.solve(assumption)) {
                break;
            }
            save_selections();
        }
    }

    for (auto &name_pair : names) {
        auto selected_dap = name_pair.second.selection;
        if (selected_dap != daps.end()) {
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
