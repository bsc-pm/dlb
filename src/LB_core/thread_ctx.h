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

#ifndef THREAD_CTX_H
#define THREAD_CTX_H

#include <stdbool.h>

typedef enum {
    THREAD_ROLE_UNKNOWN = 0,     /* default; not yet classified         */
    THREAD_ROLE_MAIN_SEQUENTIAL, /* set in DLB_init()                   */
    THREAD_ROLE_MAIN_PARALLEL,   /* detected via OMPT parallel_begin    */
    THREAD_ROLE_WORKER,          /* detected via OMPT thread_begin      */
    THREAD_ROLE_OBSERVER,        /* calls the API but is never profiled */
} thread_role_t;

extern __thread thread_role_t thread_role;

static inline bool thread_is_main(void) {
    return thread_role == THREAD_ROLE_MAIN_SEQUENTIAL ||
           thread_role == THREAD_ROLE_MAIN_PARALLEL;
}

static inline bool thread_is_main_sequential(void) {
    return thread_role == THREAD_ROLE_MAIN_SEQUENTIAL;
}

static inline bool thread_is_parallel(void) {
    return thread_role == THREAD_ROLE_MAIN_PARALLEL ||
           thread_role == THREAD_ROLE_WORKER;
}

static inline bool thread_is_profiled(void) {
    return thread_role == THREAD_ROLE_MAIN_SEQUENTIAL ||
           thread_role == THREAD_ROLE_MAIN_PARALLEL   ||
           thread_role == THREAD_ROLE_WORKER;
}

static inline bool thread_is_observer(void) {
    return thread_role == THREAD_ROLE_OBSERVER;
}

typedef enum {
    THREAD_MAIN_SEQUENTIAL,
    THREAD_MAIN_PARALLEL,
} thread_main_mode_t;

void thread_ctx_set_main(thread_main_mode_t main_mode);
void thread_ctx_set_worker(void);
void thread_ctx_set_observer(bool is_observer);

#endif /* THREAD_CTX_H */
