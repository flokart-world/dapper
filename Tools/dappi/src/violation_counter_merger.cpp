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

#include "violation_counter_merger.hpp"

violation_counter_set violation_counter_merger::pop() noexcept {
    std::pop_heap(M_queue.begin(), M_queue.end(), size_greater());
    auto result = std::move(M_queue.back());
    M_queue.pop_back();
    return result;
}

void violation_counter_merger::merge(Minisat::Solver &solver) {
    assert(!empty());

    while (M_queue.size() > 1) {
        auto lhs = pop();
        auto rhs = pop();
        auto max_violations = lhs.size() + rhs.size();

        std::vector<Minisat::Var> counters(max_violations);
        for (std::size_t index = 0; index < max_violations; ++index) {
            counters[index] = solver.newVar();
        }
        auto merged = [&](std::size_t num) {
            assert(0 < num && num <= max_violations);
            return counters[num - 1];
        };

        /* For cases violation count of rhs is zero */
        for (std::size_t index = 0; index < lhs.size(); ++index) {
            auto num = index + 1;
            /* lhs[num] implies merged[num] */
            solver.addClause(
                ~Minisat::mkLit(lhs.at_least(num)),
                Minisat::mkLit(merged(num))
            );
        }

        /* For cases violation count of lhs is zero */
        for (std::size_t index = 0; index < rhs.size(); ++index) {
            auto num = index + 1;
            /* rhs[num] implies merged[num] */
            solver.addClause(
                ~Minisat::mkLit(rhs.at_least(num)),
                Minisat::mkLit(merged(num))
            );
        }

        /* For cases both violation counts are non-zero */
        for (std::size_t lhs_idx = 0; lhs_idx < lhs.size(); ++lhs_idx) {
            std::size_t lhs_num = lhs_idx + 1;
            for (std::size_t rhs_idx = 0; rhs_idx < rhs.size(); ++rhs_idx) {
                std::size_t rhs_num = rhs_idx + 1;
                /* lhs[n] and rhs[m] implies merged[n + m] */
                solver.addClause(
                    ~Minisat::mkLit(lhs.at_least(lhs_num)),
                    ~Minisat::mkLit(rhs.at_least(rhs_num)),
                    Minisat::mkLit(merged(lhs_num + rhs_num))
                );
            }
        }

        add(violation_counter_set(std::move(counters)));
    }
}
