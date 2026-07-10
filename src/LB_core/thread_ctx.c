/*********************************************************************************/
/*  Copyright 2009-2026 Barcelona Supercomputing Center                          */
/*                                                                               */
/*  This file is part of the DLB library.                                        */
/*                                                                               */
/*  DLB is free software: you can redistribute it and/or modify                  */
/*  it under the terms of the GNU Lesser General Public License as published by  */
/*  the Free Software Foundation, either version 3 of the License, or            */
/*  (at your option) any later version.                                          */
/*                                                                               */
/*  DLB is distributed in the hope that it will be useful,                       */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*  GNU Lesser General Public License for more details.                          */
/*                                                                               */
/*  You should have received a copy of the GNU Lesser General Public License     */
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

#include "LB_core/thread_ctx.h"
#include "support/debug.h"
#include <stdbool.h>

__thread thread_role_t thread_role = THREAD_ROLE_UNKNOWN;

void thread_ctx_set_main(thread_main_mode_t main_mode) {

    ensure(thread_role != THREAD_ROLE_WORKER,
            "Wrong previous thread role in %s (%d). Please, report bug.",
            __func__, thread_role);

    thread_role =
        main_mode == THREAD_MAIN_SEQUENTIAL ? THREAD_ROLE_MAIN_SEQUENTIAL
        : main_mode == THREAD_MAIN_PARALLEL ? THREAD_ROLE_MAIN_PARALLEL
        : THREAD_ROLE_UNKNOWN;

    ensure(thread_role != THREAD_ROLE_UNKNOWN,
            "Unhandled thread main mode in %s. Please, report bug.",
            __func__);
}

void thread_ctx_set_worker(void) {

    ensure(thread_role == THREAD_ROLE_UNKNOWN,
            "Wrong previous thread role in %s (%d). Please, report bug.",
            __func__, thread_role);

    thread_role = THREAD_ROLE_WORKER;
}

void thread_ctx_set_observer(bool is_observer) {

    static __thread thread_role_t previous_thread_role = THREAD_ROLE_UNKNOWN;

    if (is_observer
            && thread_role != THREAD_ROLE_OBSERVER) {
        previous_thread_role = thread_role;
        thread_role = THREAD_ROLE_OBSERVER;
    } else if (!is_observer
            && thread_role == THREAD_ROLE_OBSERVER) {
        thread_role = previous_thread_role;
        previous_thread_role = THREAD_ROLE_UNKNOWN;
    }
}
