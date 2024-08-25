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

#ifndef VIOLATION_COUNTER_SET_HPP
#define VIOLATION_COUNTER_SET_HPP

#include <cassert>
#include <vector>
#include <minisat/core/SolverTypes.h>

/*
 * Represents a set of violation counters for solving MaxSAT problem.
 */
class violation_counter_set {
private:
    std::vector<Minisat::Var> M_violation_counters;

public:
    explicit violation_counter_set(std::vector<Minisat::Var> counters) :
            M_violation_counters(std::move(counters)) {
    }

    auto at_least(std::size_t num) const {
        assert(num > 0);
        assert(!M_violation_counters.empty());
        return M_violation_counters[num - 1];
    }

    auto begin() const {
        return M_violation_counters.begin();
    }

    auto end() const {
        return M_violation_counters.end();
    }
};

#endif
