/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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

#include "LB_policies/lewi_mask.h"

#include "LB_numThreads/numThreads.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_core/spd.h"
#include "apis/dlb_errors.h"
#include "support/debug.h"
#include "support/tracing.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static int nthreads;
static int initial_nthreads;
static int enabled = 0;
static int single = 0;
static int master_cpu;

/******* Main Functions - LeWI Mask Balancing Policy ********/

int lewi_mask_Init(const subprocess_descriptor_t *spd) {
    verbose(VB_MICROLB, "LeWI Mask Balancing Init");

    initial_nthreads = CPU_COUNT(&spd->process_mask);
    nthreads = initial_nthreads;
    add_event(THREADS_USED_EVENT, nthreads);

    enabled = 1;
    return DLB_SUCCESS;
}

int lewi_mask_Finish(const subprocess_descriptor_t *spd) {
    //cpu_set_t mask;
    //CPU_ZERO(&mask);
    shmem_cpuinfo__recover_all(spd->id, NULL);
    //if (CPU_COUNT(&mask)>0) add_mask(&spd->pm, &mask);
    return DLB_SUCCESS;
}

int lewi_mask_EnableDLB(const subprocess_descriptor_t *spd) {
    single = 0;
    enabled = 1;
    return DLB_SUCCESS;
}

int lewi_mask_DisableDLB(const subprocess_descriptor_t *spd) {
    if (enabled && !single) {
        cpu_set_t current_mask;
        memcpy(&current_mask, &spd->active_mask, sizeof(cpu_set_t));
        nthreads = shmem_cpuinfo__reset_default_cpus(spd->id, &current_mask);
        set_mask( &spd->pm, &current_mask);
    }
    enabled = 0;
    return DLB_SUCCESS;
}


int lewi_mask_IntoCommunication(const subprocess_descriptor_t *spd) { return DLB_SUCCESS; }

int lewi_mask_OutOfCommunication(const subprocess_descriptor_t *spd) { return DLB_SUCCESS; }

/* Into Blocking Call - Lend the maximum number of threads */
int lewi_mask_IntoBlockingCall(const subprocess_descriptor_t *spd) {
    if (enabled) {
        cpu_set_t mask;
        memcpy(&mask, &spd->active_mask, sizeof(cpu_set_t));

        //Check num threads and mask size are the same
        assert(nthreads==CPU_COUNT(&mask));

        cpu_set_t cpu;
        CPU_ZERO( &cpu );
        sched_getaffinity( 0, sizeof(cpu_set_t), &cpu);
        master_cpu = sched_getcpu();

        if ( spd->options.mpi_lend_mode == ONE_CPU ) {
            // Remove current cpu from the mask
            CPU_XOR( &mask, &mask, &cpu );
            verbose( VB_MICROLB, "LENDING %d threads", nthreads-1 );
            nthreads = 1;
        } else if ( spd->options.mpi_lend_mode == BLOCK ) {
            verbose( VB_MICROLB, "LENDING %d threads", nthreads );
            nthreads = 0;
        }

        // Don't set application mask if it is already 1 cpu
        if ( !CPU_EQUAL( &mask, &cpu ) ) {
            set_mask( &spd->pm, &cpu );
        }

        // Don't add mask if empty
        if ( CPU_COUNT( &mask ) > 0 ) {
            shmem_cpuinfo__add_cpu_mask(spd->id, &mask, NULL);
        }

        add_event( THREADS_USED_EVENT, nthreads );
        //Check num threads and mask size are the same (this is the only case where they don't match)
        //assert(nthreads+1==CPU_COUNT(&cpu));
    }
    return DLB_SUCCESS;
}

/* Out of Blocking Call - Recover the default number of threads */
int lewi_mask_OutOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter) {
    if (enabled) {
        cpu_set_t mask;
        memcpy(&mask, &spd->active_mask, sizeof(cpu_set_t));
        //Check num threads and mask size are the same
        //assert(nthreads+1==CPU_COUNT(&mask));

        if (single) {
            verbose(VB_MICROLB, "RECOVERING master thread" );
            shmem_cpuinfo__recover_cpu(spd->id, master_cpu, NULL);
            CPU_SET( master_cpu, &mask );
            nthreads = 1;
        } else {
            verbose(VB_MICROLB, "RECOVERING %d threads", initial_nthreads - nthreads );
            shmem_cpuinfo__recover_all(spd->id, NULL);
            nthreads = initial_nthreads;
        }
        set_mask( &spd->pm, &mask );
        assert(nthreads==CPU_COUNT(&mask));
        add_event( THREADS_USED_EVENT, nthreads );
    }
    return DLB_SUCCESS;
}

int lewi_mask_Lend(const subprocess_descriptor_t *spd) {
    lewi_mask_IntoBlockingCall(spd);
    return DLB_SUCCESS;
}

int lewi_mask_ReclaimCpus(const subprocess_descriptor_t *spd, int ncpus) {
    int error = DLB_ERR_PERM;
    if (enabled && !single) {
        if (nthreads<initial_nthreads) {
            //Do not get more cpus than the default ones

            if ((ncpus+nthreads)>initial_nthreads) { ncpus=initial_nthreads-nthreads; }

            verbose( VB_MICROLB, "Claiming %d cpus", ncpus );
            cpu_set_t current_mask;
            memcpy(&current_mask, &spd->active_mask, sizeof(cpu_set_t));

            //Check num threads and mask size are the same
            assert(nthreads==CPU_COUNT(&current_mask));

            shmem_cpuinfo__recover_cpus(spd->id, ncpus, NULL);
            set_mask( &spd->pm, &current_mask);
            nthreads += ncpus;
            assert(nthreads==CPU_COUNT(&current_mask));

            add_event( THREADS_USED_EVENT, nthreads );
            error = DLB_SUCCESS;
        }
    } else {
        error = DLB_ERR_DISBLD;
    }
    return error;
}

int lewi_mask_AcquireCpu(const subprocess_descriptor_t *spd, int cpuid) {
    int error = DLB_ERR_PERM;
    if (enabled && !single) {
        verbose(VB_MICROLB, "AcquireCpu %d", cpuid);
        cpu_set_t mask;
        memcpy(&mask, &spd->active_mask, sizeof(cpu_set_t));

        if (!CPU_ISSET(cpuid, &mask)){

            if (shmem_cpuinfo__borrow_cpu(spd->id, cpuid, NULL)) {
                nthreads++;
                CPU_SET(cpuid, &mask);
                set_mask( &spd->pm, &mask );
                verbose(VB_MICROLB, "New Mask: %s", mu_to_str(&mask));
                add_event( THREADS_USED_EVENT, nthreads );
                error = DLB_SUCCESS;
            }else{
                verbose(VB_MICROLB, "Not legally acquired CPU %d, running anyway",cpuid);
            }
        }
    } else {
        error = DLB_ERR_DISBLD;
    }
    return error;
}

int lewi_mask_AcquireCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t* mask) {
    int error = DLB_ERR_PERM;
    if (enabled && !single) {
        verbose(VB_MICROLB, "AcquireCpus %s", mu_to_str(mask));
        cpu_set_t current_mask;
        memcpy(&current_mask, &spd->active_mask, sizeof(cpu_set_t));

        bool success = false;
        int cpu;
        for (cpu = 0; cpu < mu_get_system_size(); ++cpu) {
            if (!CPU_ISSET(cpu, &current_mask)
                    && CPU_ISSET(cpu, mask)){

                if (shmem_cpuinfo__borrow_cpu(spd->id, cpu, NULL)) {
                    nthreads++;
                    CPU_SET(cpu, &current_mask);
                    success = true;
                } else {
                    verbose(VB_MICROLB, "Not legally acquired cpu %d, running anyway",cpu);
                }
            }
        }
        if (success) {
            set_mask( &spd->pm, &current_mask );
            verbose(VB_MICROLB, "New Mask: %s", mu_to_str(&current_mask));
            add_event( THREADS_USED_EVENT, nthreads );
            error = DLB_SUCCESS;
        }
    } else {
        error = DLB_ERR_DISBLD;
    }
    return error;
}

/* Try to acquire foreign threads */
int lewi_mask_BorrowCpus(const subprocess_descriptor_t *spd, int ncpus) {
    int error = DLB_ERR_PERM;
    if (enabled && !single) {
        cpu_set_t mask;
        memcpy(&mask, &spd->active_mask, sizeof(cpu_set_t));

        //Check num threads and mask size are the same
        assert(nthreads==CPU_COUNT(&mask));

        int collected = shmem_cpuinfo__collect_mask(spd->id, &mask, ncpus,
                spd->options.priority);

        if ( collected > 0 ) {
            nthreads += collected;
            set_mask(&spd->pm, &mask);
            verbose( VB_MICROLB, "ACQUIRING %d threads for a total of %d", collected, nthreads );
            add_event( THREADS_USED_EVENT, nthreads );
            error = DLB_SUCCESS;
        }
        //Check num threads and mask size are the same
        assert(nthreads==CPU_COUNT(&mask));
    } else {
        error = DLB_ERR_DISBLD;
    }
    return error;
}

/* Return foreign threads that have been claimed by its owner */
int lewi_mask_ReturnAll(const subprocess_descriptor_t *spd) {
    int error = DLB_ERR_PERM;
    if (enabled && !single) {
        cpu_set_t mask;
        memcpy(&mask, &spd->active_mask, sizeof(cpu_set_t));

        //Check num threads and mask size are the same
        assert(nthreads==CPU_COUNT(&mask));

        int returned = shmem_cpuinfo__return_claimed(spd->id, &mask);

        if ( returned > 0 ) {
            nthreads -= returned;
            set_mask(&spd->pm, &mask);
            verbose( VB_MICROLB, "RETURNING %d threads for a total of %d", returned, nthreads );
            add_event( THREADS_USED_EVENT, nthreads );
            error = DLB_SUCCESS;
        }
        //Check num threads and mask size are the same
        assert(nthreads==CPU_COUNT(&mask));
    } else {
        error = DLB_ERR_DISBLD;
    }
    return error;
}

