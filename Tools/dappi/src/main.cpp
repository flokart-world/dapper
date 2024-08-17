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
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <semver.hpp>
#include <minisat/core/Solver.h>

namespace {

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

} // namespace

int main() {
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

    /*
     * Defining penality counters.
     *
     * Assume least_partial_penalty_count(last, num) to be true if there
     * are at least num true penalties within range [0..last] of defined
     * penalties.
     *
     * least_partial_penalty_count(last, 0) is always true for any last.
     *
     * Where last = 0 and num = 1, (Case A)
     * least_partial_penalty_count(last, num) implies penalty[last].
     *
     * Where last > 0 and 0 < num <= last, (Case B)
     * least_partial_penalty_count(last, num) implies
     *   least_partial_penalty_count(last - 1, num) or
     *   penalty[last] and least_partial_penalty_count(last - 1, num - 1).
     *
     * For the special case where last > 0 and num = 1, (Case B')
     * least_partial_penalty_count(last, num) implies
     *   least_partial_penalty_count(last - 1, num) or penalty[last].
     *
     * Where last > 0 and num = last + 1, (Case C)
     * least_partial_penalty_count(last, num) implies
     *   penalty[last] and least_partial_penalty_count(last - 1, num - 1).
     */
    if (!penalties.empty()) {
        std::vector<Minisat::Var> partial_penalty_counters;
        partial_penalty_counters.resize(
            penalties.size() * (penalties.size() + 1) / 2
        );
        auto least_partial_count = [&](
            std::size_t last,
            std::size_t num
        ) -> decltype (auto) {
            return partial_penalty_counters [last * (last + 1) / 2 + num - 1];
        };
        auto least_count = [&](std::size_t num) {
            return least_partial_count(penalties.size() - 1, num);
        };

        Minisat::Var new_var;

        // Case A
        new_var = resolution.newVar();
        resolution.addClause(
            ~Minisat::mkLit(new_var),
            Minisat::mkLit(penalties[0])
        );
        least_partial_count(0, 1) = new_var;

        for (std::size_t last = 1; last < penalties.size(); ++last) {
            // Case B'
            new_var = resolution.newVar();
            resolution.addClause(
                ~Minisat::mkLit(new_var),
                Minisat::mkLit(least_partial_count(last - 1, 1)),
                Minisat::mkLit(penalties[last])
            );
            least_partial_count(last, 1) = new_var;

            // Case B
            for (std::size_t num = 2; num <= last; ++num) {
                new_var = resolution.newVar();
                resolution.addClause(
                    ~Minisat::mkLit(new_var),
                    Minisat::mkLit(least_partial_count(last - 1, num)),
                    Minisat::mkLit(penalties[last])
                );
                resolution.addClause(
                    ~Minisat::mkLit(new_var),
                    Minisat::mkLit(least_partial_count(last - 1, num)),
                    Minisat::mkLit(least_partial_count(last - 1, num - 1))
                );
                least_partial_count(last, num) = new_var;
            }

            // Case C
            new_var = resolution.newVar();
            resolution.addClause(
                ~Minisat::mkLit(new_var),
                Minisat::mkLit(penalties[last])
            );
            resolution.addClause(
                ~Minisat::mkLit(new_var),
                Minisat::mkLit(least_partial_count(last - 1, last))
            );
            least_partial_count(last, last + 1) = new_var;
        }

        /*
         * Now we improve the model by adding assumption.
         */
        for (size_t past_num = penalties.size(); past_num > 0; --past_num) {
            auto assumption = ~Minisat::mkLit(least_count(past_num - 1));
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
