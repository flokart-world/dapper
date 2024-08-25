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

#ifndef VIOLATION_COUNTER_MERGER_HPP
#define VIOLATION_COUNTER_MERGER_HPP

#include <cassert>
#include <queue>
#include <minisat/core/Solver.h>
#include "violation_counter_set.hpp"

class violation_counter_merger {
private:
    struct size_greater {
        bool operator ()(
            const violation_counter_set &lhs,
            const violation_counter_set &rhs
        ) const noexcept {
            return (lhs.size() > rhs.size());
        }
    };

    std::deque<violation_counter_set> M_queue;

    violation_counter_set pop() noexcept;

public:
    bool empty() const noexcept {
        return M_queue.empty();
    }

    auto release() noexcept {
        assert(M_queue.size() == 1);
        auto result = std::move(M_queue.back());
        M_queue.pop_back();
        return result;
    }

    void add(violation_counter_set group) {
        M_queue.push_back(std::move(group));
        std::push_heap(M_queue.begin(), M_queue.end(), size_greater());
    }

    void merge(Minisat::Solver &solver);
};

#endif
