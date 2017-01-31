/*********************************************************************************/
/*  Copyright 2015 Barcelona Supercomputing Center                               */
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
/*  along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*********************************************************************************/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <unistd.h>

#include "LB_core/spd.h"
#include "LB_core/DLB_kernel.h"
#include "LB_core/statistics.h"
#include "LB_numThreads/numThreads.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_comm/shmem_barrier.h"
#include "support/debug.h"
#include "support/error.h"
#include "support/globals.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mytime.h"
#include "support/mask_utils.h"
#include "support/sighandler.h"


/* These flags are used to
 *  a) activate/deactive DLB functionality from the API
 *  b) protect DLB functionality before the initialization
 *  c) protect DLB_Init / DLB_MPI_Init twice
 */
static bool dlb_enabled = false;
static bool dlb_initialized = false;

/* Temporary global sub-process descriptor */
spd_t global_spd;


static void print_summary() {
        verbose(VB_API, "Enabled verbose mode for DLB API");
        verbose(VB_MPI_API, "Enabled verbose mode for MPI API");
        verbose(VB_MPI_INT, "Enabled verbose mode for MPI Interception");
        verbose(VB_SHMEM, "Enabled verbose mode for Shared Memory");
        verbose(VB_DROM, "Enabled verbose mode for DROM");
        verbose(VB_STATS, "Enabled verbose mode for STATS");
        verbose(VB_MICROLB, "Enabled verbose mode for microLB policies");
#ifdef MPI_LIB
        info0 ( "MPI processes per node: %d", _mpis_per_node );
#endif
        info0("%s %s", PACKAGE, VERSION);
//      info0( "Balancing policy: <name>" );
        info0("This process starts with %d threads", _default_nthreads);

        if (global_spd.options.mpi_just_barrier)
            info0("Only lending resources when MPI_Barrier "
                    "(Env. var. LB_JUST_BARRIER is set)" );

        if (global_spd.options.mpi_lend_mode == BLOCK)
            info0("LEND mode set to BLOCKING. I will lend all "
                    "the resources when in an MPI call" );
}

void set_dlb_enabled(bool enabled) {
    if (dlb_initialized) {
        dlb_enabled = enabled;
        if (enabled){
            global_spd.lb_funcs.enableDLB();
        }else{
            global_spd.lb_funcs.disableDLB();
        }
    }
}


int Initialize(const cpu_set_t *mask, const char *lb_args) {

    int error = DLB_SUCCESS;
    if (!dlb_initialized) {
        // Initialize first instrumentation module
        options_init(&global_spd.options, lb_args);
        init_tracing(&global_spd.options);
        add_event(RUNTIME_EVENT, EVENT_INIT);

        // Initialize the rest of the subprocess descriptor
        pm_init(&global_spd.pm);
        policy_t policy = global_spd.options.lb_policy;
        set_lb_funcs(&global_spd.lb_funcs, policy);
        global_spd.process_id = getpid();
        global_spd.use_dpd = (
                policy == POLICY_RAL || policy == POLICY_WEIGHT || policy == POLICY_JUST_PROF);
        if (mask) {
            memcpy(&global_spd.process_mask, mask, sizeof(cpu_set_t));
        } else {
            sched_getaffinity(0, sizeof(cpu_set_t), &global_spd.process_mask);
        }


        // Initialize modules
        debug_init(&global_spd.options);
        global_spd.lb_funcs.init();
        if (policy != POLICY_NONE || global_spd.options.drom || global_spd.options.statistics) {
            shmem_procinfo__init(&global_spd.process_mask);
            shmem_cpuinfo__init(&global_spd.process_mask);
        }
        if (global_spd.options.barrier) {
            shmem_barrier_init();
        }

        dlb_enabled = true;
        dlb_initialized = true;
        add_event(DLB_MODE_EVENT, EVENT_ENABLED);
        add_event(RUNTIME_EVENT, 0);

        print_summary();
    } else {
        error = DLB_ERR_INIT;
    }
    return error;
}


int Finish(void) {

    int error = DLB_SUCCESS;
    if (dlb_initialized) {
        dlb_enabled = false;
        dlb_initialized = false;
        global_spd.lb_funcs.finish();
        // Unload modules
        if (global_spd.options.barrier) {
            shmem_barrier_finalize();
        }
        policy_t policy = global_spd.options.lb_policy;
        if (policy != POLICY_NONE || global_spd.options.drom || global_spd.options.statistics) {
            shmem_cpuinfo__finalize();
            shmem_procinfo__finalize();
        }
    } else {
        error = DLB_ERR_NOINIT;
    }
    return error;
}

void Update(void) {
    if (dlb_enabled) {
        shmem_procinfo__update(global_spd.options.drom, global_spd.options.statistics);
    }
}

// FIXME header
int shmem_procinfo__update_new(bool do_drom, bool do_stats, int *new_threads, cpu_set_t *new_mask);
int Update_new(int *new_threads, cpu_set_t *new_mask) {
    int error = DLB_ERR_UNKNOWN;
    if (dlb_enabled) {
        cpu_set_t *mask = (new_mask != NULL) ? new_mask : malloc(sizeof(cpu_set_t));
        error = shmem_procinfo__update_new(
                global_spd.options.drom, global_spd.options.statistics, new_threads, mask);
        if (error == DLB_SUCCESS) {
            // can this function return error?
            shmem_cpuinfo__update_ownership(mask);
        }
    } else {
        error = DLB_ERR_DISBLD;
    }
    return error;
}

void IntoCommunication(void) {
    /*  struct timespec aux;
        clock_gettime(CLOCK_REALTIME, &initMPI);
        diff_time(initComp, initMPI, &aux);
        add_time(iterCpuTime, aux, &iterCpuTime);*/

    if (dlb_enabled) {
        global_spd.lb_funcs.intoCommunication();
    }
}

void OutOfCommunication(void) {
    /*  struct timespec aux;
        clock_gettime(CLOCK_REALTIME, &initComp);
        diff_time(initMPI, initComp, &aux);
        add_time(iterMPITime, aux, &iterMPITime);*/

    if (dlb_enabled) {
        global_spd.lb_funcs.outOfCommunication();
    }
}

void IntoBlockingCall(int is_iter, int blocking_mode) {
    /*  double cpuSecs;
        double MPISecs;
        cpuSecs=iterCpuTime_avg;
        MPISecs=iterMPITime_avg;*/
    if (dlb_enabled) {
        // We explicitly ignore the argument
        global_spd.lb_funcs.intoBlockingCall(is_iter, global_spd.options.mpi_lend_mode);
    }
}

void OutOfBlockingCall(int is_iter) {
    if (dlb_enabled) {
        global_spd.lb_funcs.outOfBlockingCall(is_iter);
    }
}

void updateresources(int max_resources) {
    if (dlb_enabled) {
        add_event(RUNTIME_EVENT, EVENT_UPDATE);
        global_spd.lb_funcs.updateresources( max_resources );
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
}

void returnclaimed(void) {
    if (dlb_enabled) {
        add_event(RUNTIME_EVENT, EVENT_RETURN);
        global_spd.lb_funcs.returnclaimed();
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
}

int releasecpu(int cpu) {
    if (dlb_enabled) {
        add_event(RUNTIME_EVENT, EVENT_RELEASE_CPU);
        int error = global_spd.lb_funcs.releasecpu(cpu);
        add_event(RUNTIME_EVENT, EVENT_USER);
        return error;
    } else {
        return 0;
    }
}

int returnclaimedcpu(int cpu) {
    if (dlb_enabled) {
        add_event(RUNTIME_EVENT, EVENT_RETURN_CPU);
        int error = global_spd.lb_funcs.returnclaimedcpu(cpu);
        add_event(RUNTIME_EVENT, EVENT_USER);
        return error;
    } else {
        return 0;
    }
}

void claimcpus(int cpus) {
    if (dlb_enabled) {
        add_event(RUNTIME_EVENT, EVENT_CLAIM_CPUS);
        global_spd.lb_funcs.claimcpus(cpus);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
}

void acquirecpu(int cpu){
    if (dlb_enabled) {
        add_event(RUNTIME_EVENT, EVENT_ACQUIRE_CPU);
        global_spd.lb_funcs.acquirecpu(cpu);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
}

void acquirecpus(cpu_set_t* mask){
    if (dlb_enabled) {
        add_event(RUNTIME_EVENT, EVENT_ACQUIRE_CPU);
        global_spd.lb_funcs.acquirecpus(mask);
        add_event(RUNTIME_EVENT, EVENT_USER);
    }
}

int checkCpuAvailability(int cpu) {
    if (dlb_enabled) {
        return global_spd.lb_funcs.checkCpuAvailability(cpu);
    } else {
        return 1;
    }
}

void resetDLB(void) {
    if (dlb_enabled) {
        global_spd.lb_funcs.resetDLB();
    }
}

void singlemode(void) {
    dlb_enabled = false;
    global_spd.lb_funcs.single();
}

void parallelmode(void) {
    dlb_enabled = true;
    global_spd.lb_funcs.parallel();
}

void nodebarrier(void) {
    shmem_barrier();
}

void notifymaskchangeto(const cpu_set_t* mask) {
    shmem_cpuinfo__update_ownership(mask);
}

void notifymaskchange(void) {
    cpu_set_t process_mask;
    get_process_mask(&global_spd.pm, &process_mask);
    notifymaskchangeto(&process_mask);
}

void printShmem(void) {
    pm_init(&global_spd.pm);
    options_init(&global_spd.options, NULL);
    debug_init(&global_spd.options);
    shmem_cpuinfo_ext__init();
    shmem_cpuinfo_ext__print_info();
    shmem_cpuinfo_ext__finalize();
    shmem_procinfo_ext__init();
    shmem_procinfo_ext__print_info();
    shmem_procinfo_ext__finalize();
}
