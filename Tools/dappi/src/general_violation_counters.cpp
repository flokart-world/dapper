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

#include "general_violation_counters.hpp"

#include <cassert>

violation_counter_set make_general_violation_counters(
    Minisat::Solver &solver,
    const std::vector<Minisat::Var> &violations
) {
    /*
     * Assume least_partial_count(last, num) to be true if there are at least
     * num violations within range [0..last] of defined soft clauses.
     *
     * least_partial_count(last, 0) is always true for any last.
     *
     * Where last = 0 and num = 1, (Case A)
     * least_partial_count(last, num) is implied by violation[last].
     *
     * Where last > 0 and 0 < num <= last, (Case B)
     * least_partial_count(last, num) is implied by
     *   least_partial_count(last - 1, num) or
     *   violation[last] and least_partial_count(last - 1, num - 1).
     *
     * For the special case where last > 0 and num = 1, (Case B')
     * least_partial_count(last, num) is implied by
     *   least_partial_count(last - 1, num) or violation[last].
     *
     * Where last > 0 and num = last + 1, (Case C)
     * least_partial_count(last, num) is implied by
     *   violation[last] and least_partial_count(last - 1, last).
     */

    if (violations.empty()) {
        return violation_counter_set({});
    }

    /* [index] corresponds to least_partial_count(last - 1, index + 1) */
    std::vector<Minisat::Var> previous_counters;

    /* [index] corresponds to least_partial_count(last, index + 1) */
    std::vector<Minisat::Var> current_counters;

    Minisat::Var new_var;

    // Case A
    new_var = solver.newVar();
    solver.addClause(
        Minisat::mkLit(new_var),
        ~Minisat::mkLit(violations[0])
    );
    current_counters.resize(1);
    current_counters[0] = new_var;

    for (std::size_t last = 1; last < violations.size(); ++last) {
        previous_counters = std::move(current_counters);
        current_counters.resize(last + 1);

        // Case B'
        new_var = solver.newVar();
        solver.addClause(
            Minisat::mkLit(new_var),
            ~Minisat::mkLit(previous_counters[0])
        );
        solver.addClause(
            Minisat::mkLit(new_var),
            ~Minisat::mkLit(violations[last])
        );
        current_counters[0] = new_var;

        // Case B
        for (std::size_t num = 2; num <= last; ++num) {
            new_var = solver.newVar();
            solver.addClause(
                Minisat::mkLit(new_var),
                ~Minisat::mkLit(previous_counters[num - 1])
            );
            solver.addClause(
                Minisat::mkLit(new_var),
                ~Minisat::mkLit(violations[last]),
                ~Minisat::mkLit(previous_counters[num - 2])
            );
            current_counters[num - 1] = new_var;
        }

        // Case C
        new_var = solver.newVar();
        solver.addClause(
            Minisat::mkLit(new_var),
            ~Minisat::mkLit(violations[last]),
            ~Minisat::mkLit(previous_counters[last - 1])
        );
        current_counters[last] = new_var;
    }

    return violation_counter_set(std::move(current_counters));
}
