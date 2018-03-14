/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

#include "LB_numThreads/ompt.h"

#include "apis/dlb.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/tracing.h"
#include "apis/dlb_errors.h"

#include <inttypes.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>

int omp_get_thread_num(void) __attribute__((weak));
void omp_set_num_threads(int nthreads) __attribute__((weak));
int omp_get_level(void) __attribute__((weak));


/*********************************************************************************/
/*  OMP Thread Manager                                                           */
/*********************************************************************************/

static cpu_set_t active_mask;
static bool lewi = false;

static void cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &active_mask);
}

static void omp_thread_manager_acquire(void) {
    if (lewi) {
        DLB_Borrow();
        int nthreads = CPU_COUNT(&active_mask);
        omp_set_num_threads(nthreads);
        verbose(VB_OMPT, "Acquire - Setting new mask to %s", mu_to_str(&active_mask));
    }
}

static void omp_thread_manager_release(void) {
    if (lewi) {
        omp_set_num_threads(1);
        CPU_ZERO(&active_mask);
        CPU_SET(sched_getcpu(), &active_mask);
        verbose(VB_OMPT, "Release - Setting new mask to %s", mu_to_str(&active_mask));
        DLB_Lend();
    }
}

static void omp_thread_manager_init(void) {
    if (lewi) {
        cpu_set_t process_mask;
        sched_getaffinity(0, sizeof(cpu_set_t), &process_mask);
        DLB_Init(0, &process_mask, NULL);
        DLB_CallbackSet(dlb_callback_enable_cpu, (dlb_callback_t)cb_enable_cpu, NULL);
        omp_thread_manager_release();
    }
}

static void omp_thread_manager_finalize(void) {
    if (lewi) {
        DLB_Finalize();
    }
}


/*********************************************************************************/
/*  OMPT callbacks                                                               */
/*********************************************************************************/

static void cb_parallel_begin(
        ompt_data_t *encountering_task_data,
        const ompt_frame_t *encountering_task_frame,
        ompt_data_t *parallel_data,
        unsigned int requested_team_size,
        ompt_invoker_t invoker,
        const void *codeptr_ra) {
    if (omp_get_level() == 0) {
        omp_thread_manager_acquire();
    }
    /* warning("[%d] Encountered Parallel Construct, requested size: %u, invoker: %s", */
    /*         omp_get_thread_num(), */
    /*         requested_team_size, invoker == ompt_invoker_program ? "program" : "runtime"); */
    /* if (requested_team_size == 4) { */
    /*     warning("Modifying team size 4 -> 3"); */
    /*     omp_set_num_threads(3); */
    /* } */
}

static void cb_parallel_end(
        ompt_data_t *parallel_data,
        ompt_data_t *encountering_task_data,
        ompt_invoker_t invoker,
        const void *codeptr_ra) {
    if (omp_get_level() == 0) {
        omp_thread_manager_release();
    }
    /* warning("[%d] End of Parallel Construct, invoker: %s", omp_get_thread_num(), */
    /*         invoker == ompt_invoker_program ? "program" : "runtime"); */
}

static void cb_implicit_task(
        ompt_scope_endpoint_t endpoint,
        ompt_data_t *parallel_data,
        ompt_data_t *task_data,
        unsigned int team_size,
        unsigned int thread_num) {
    if (endpoint == ompt_scope_begin) {
        if (shmem_cpuinfo__is_dirty()) {
            int cpuid = shmem_cpuinfo__get_thread_binding(getpid(), thread_num);
            if (cpuid >= 0) {
                cpu_set_t thread_mask;
                CPU_ZERO(&thread_mask);
                CPU_SET(cpuid, &thread_mask);
                pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &thread_mask);
                add_event(REBIND_EVENT, cpuid);
                verbose(VB_OMPT, "Rebinding thread %d to CPU %d", thread_num, cpuid);
            }
        }
    }
}

static void cb_thread_begin(
        ompt_thread_type_t thread_type,
        ompt_data_t *thread_data) {
    /* warning("Starting thread of type: %s", thread_type == ompt_thread_initial ? "initial" : */
    /*         thread_type == ompt_thread_worker ? "worker" : "other" ); */
}

static void cb_thread_end(
        ompt_data_t *thread_data) {
    /* warning("Ending thread"); */
}


/*********************************************************************************/
/*  OMPT start tool                                                              */
/*********************************************************************************/

static inline void set_ompt_callback(ompt_set_callback_t set_callback_fn,
        ompt_callbacks_t which, ompt_callback_t callback) {
    switch (set_callback_fn(which, callback)) {
        case ompt_set_error:
            verbose(VB_OMPT, "OMPT callback %d failed\n", which);
            break;
        case ompt_set_never:
            verbose(VB_OMPT, "OMPT callback %d registered but will never be called\n", which);
            break;
        default:
            verbose(VB_OMPT, "OMPT callback %d succesfully registered", which);
    }
}

static int ompt_initialize(ompt_function_lookup_t ompt_fn_lookup, ompt_data_t *tool_data) {
    /* Initialize minimal modules */
    options_t options;
    options_init(&options, NULL);
    debug_init(&options);
    verbose(VB_OMPT, "Initializing OMPT module");

    /* Enable experimental LeWI features only if requested */
    lewi = options.lewi && options.debug_opts & DBG_LEWI_OMPT;

    ompt_set_callback_t set_callback_fn = (ompt_set_callback_t)ompt_fn_lookup("ompt_set_callback");
    if (set_callback_fn) {
        set_ompt_callback(set_callback_fn,
                ompt_callback_thread_begin,   (ompt_callback_t)cb_thread_begin);
        set_ompt_callback(set_callback_fn,
                ompt_callback_thread_end,     (ompt_callback_t)cb_thread_end);
        set_ompt_callback(set_callback_fn,
                ompt_callback_parallel_begin, (ompt_callback_t)cb_parallel_begin);
        set_ompt_callback(set_callback_fn,
                ompt_callback_parallel_end,   (ompt_callback_t)cb_parallel_end);
        set_ompt_callback(set_callback_fn,
                ompt_callback_implicit_task,  (ompt_callback_t)cb_implicit_task);

        omp_thread_manager_init();
    } else {
        verbose(VB_OMPT, "Could not look up function \"ompt_set_callback\"");
    }

    // return a non-zero value to activate the tool
    return 1;
}

static void ompt_finalize(ompt_data_t *tool_data) {
    verbose(VB_OMPT, "Finalizing OMPT module");
    omp_thread_manager_finalize();
}


#pragma GCC visibility push(default)
ompt_start_tool_result_t* ompt_start_tool(unsigned int omp_version, const char *runtime_version) {
    static ompt_start_tool_result_t ompt_start_tool_result = {
        .initialize = ompt_initialize,
        .finalize   = ompt_finalize,
        .tool_data  = {0}
    };
    return &ompt_start_tool_result;
}
#pragma GCC visibility pop

/*********************************************************************************/

#ifdef DEBUG_VERSION
/* Static type checking */
static __attribute__((unused)) void ompt_type_checking(void) {
    { ompt_initialize_t                 __attribute__((unused)) fn = ompt_initialize; }
    { ompt_finalize_t                   __attribute__((unused)) fn = ompt_finalize; }
    { ompt_callback_thread_begin_t      __attribute__((unused)) fn = cb_thread_begin; }
    { ompt_callback_thread_end_t        __attribute__((unused)) fn = cb_thread_end; }
    { ompt_callback_parallel_begin_t    __attribute__((unused)) fn = cb_parallel_begin; }
    { ompt_callback_parallel_end_t      __attribute__((unused)) fn = cb_parallel_end; }
    { ompt_callback_implicit_task_t     __attribute__((unused)) fn = cb_implicit_task; }
}
#endif
