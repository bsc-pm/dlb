/*********************************************************************************/
/*  Copyright 2009-2019 Barcelona Supercomputing Center                          */
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

#include "LB_numThreads/omp_thread_manager.h"

#include "apis/dlb.h"
#include "support/debug.h"
#include "support/types.h"
#include "support/mask_utils.h"
#include "LB_comm/shmem_procinfo.h"

#include <sched.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>

void omp_set_num_threads(int nthreads) __attribute__((weak));


/*********************************************************************************/
/*  OMP Thread Manager                                                           */
/*********************************************************************************/

static cpu_set_t active_mask, process_mask;
static bool lewi = false;
static pid_t pid;
static ompt_opts_t ompt_opts;

static void cb_enable_cpu(int cpuid, void *arg) {
    CPU_SET(cpuid, &active_mask);
    omp_set_num_threads(CPU_COUNT(&active_mask));
}

static void cb_disable_cpu(int cpuid, void *arg) {
    CPU_CLR(cpuid, &active_mask);
    omp_set_num_threads(CPU_COUNT(&active_mask));
}

static void cb_set_process_mask(const cpu_set_t *mask, void *arg) {
    memcpy(&process_mask, mask, sizeof(cpu_set_t));
    memcpy(&active_mask, mask, sizeof(cpu_set_t));
    omp_set_num_threads(CPU_COUNT(&active_mask));
}


void omp_thread_manager__init(const options_t *options) {
    lewi = options->lewi;
    ompt_opts = options->lewi_ompt;
    pid = options->preinit_pid ? options->preinit_pid : getpid();
    if (lewi) {
        int err;
        err = DLB_CallbackSet(dlb_callback_enable_cpu, (dlb_callback_t)cb_enable_cpu, NULL);
        if (err != DLB_SUCCESS) {
            warning("DLB_CallbackSet enable_cpu: %s", DLB_Strerror(err));
        }
        err = DLB_CallbackSet(dlb_callback_disable_cpu, (dlb_callback_t)cb_disable_cpu, NULL);
        if (err != DLB_SUCCESS) {
            warning("DLB_CallbackSet disable_cpu: %s", DLB_Strerror(err));
        }
        err = DLB_CallbackSet(dlb_callback_set_process_mask,
                (dlb_callback_t)cb_set_process_mask, NULL);
        if (err != DLB_SUCCESS) {
            warning("DLB_CallbackSet set_process_mask: %s", DLB_Strerror(err));
        }
        shmem_procinfo__getprocessmask(pid, &process_mask, 0);
        memcpy(&active_mask, &process_mask, sizeof(cpu_set_t));
        verbose(VB_OMPT, "Initial mask set to: %s", mu_to_str(&process_mask));
        omp_thread_manager__lend();
    }
}

void omp_thread_manager__finalize(void) {
}

void omp_thread_manager__borrow(void) {
    static int cpus_to_borrow = 1;

    if (lewi && ompt_opts & OMPT_OPTS_BORROW) {
        int err_return = DLB_Return();
        int err_reclaim = DLB_Reclaim();
        int err_borrow = DLB_BorrowCpus(cpus_to_borrow);

        if (err_return == DLB_SUCCESS
                || err_reclaim == DLB_SUCCESS
                || err_borrow == DLB_SUCCESS) {
            verbose(VB_OMPT, "Acquire - Setting new mask to %s", mu_to_str(&active_mask));
        }

        if (err_borrow == DLB_SUCCESS) {
            ++cpus_to_borrow;
        } else {
            cpus_to_borrow = 1;
        }

    }
}

void omp_thread_manager__lend(void) {
    if (lewi && ompt_opts & OMPT_OPTS_LEND) {
        omp_set_num_threads(1);
        CPU_ZERO(&active_mask);
        CPU_SET(sched_getcpu(), &active_mask);
        verbose(VB_OMPT, "Release - Setting new mask to %s", mu_to_str(&active_mask));
        DLB_SetMaxParallelism(1);
    }
}

/* lb_funcs.into_blocking_call has already been called and
 * the current CPU will be lent according to the --lew-mpi option
 * This function just lends the rest of the CPUs
 */
void omp_thread_manager__IntoBlockingCall(void) {
    if (lewi && ompt_opts & OMPT_OPTS_MPI) {
        int mycpu = sched_getcpu();

        /* Lend every CPU except the current one */
        cpu_set_t mask;
        memcpy(&mask, &active_mask, sizeof(cpu_set_t));
        CPU_CLR(mycpu, &mask);
        DLB_LendCpuMask(&mask);

        /* Set active_mask to only the current CPU */
        CPU_ZERO(&active_mask);
        CPU_SET(mycpu, &active_mask);
        omp_set_num_threads(1);

        verbose(VB_OMPT, "IntoBlockingCall - lending all");
    }
}

void omp_thread_manager__OutOfBlockingCall(void) {
    if (lewi && ompt_opts & OMPT_OPTS_MPI) {
        DLB_Reclaim();
    }
}


