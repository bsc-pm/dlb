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
#include <sched.h>
#include <stdlib.h>
#include <strings.h>
#include <assert.h>

#include "LB_numThreads/numThreads.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "support/debug.h"
#include "support/globals.h"
#include "support/tracing.h"
#include "support/mask_utils.h"


static int nthreads;
static int enabled = 0;
static int single = 0;
static int master_cpu;

/******* Main Functions - LeWI Mask Balancing Policy ********/

void lewi_mask_Init(void) {
    verbose(VB_MICROLB, "LeWI Mask Balancing Init");

    nthreads = _default_nthreads;
    add_event(THREADS_USED_EVENT, nthreads);

    //Check num threads and mask size are the same
    cpu_set_t default_mask;
    get_mask(&default_mask);
    assert(nthreads==CPU_COUNT(&default_mask));

    enabled = 1;
}

void lewi_mask_Finish(void) {
    cpu_set_t mask;
    CPU_ZERO(&mask);
    shmem_cpuinfo__recover_some_cpus(&mask, CPUINFO_RECOVER_ALL);
    if (CPU_COUNT(&mask)>0) add_mask(&mask);
}

void lewi_mask_IntoCommunication(void) {}

void lewi_mask_OutOfCommunication(void) {}

/* Into Blocking Call - Lend the maximum number of threads */
void lewi_mask_IntoBlockingCall(int is_iter, int blocking_mode) {
    if (enabled) {
        cpu_set_t mask;
        CPU_ZERO( &mask );
        get_mask( &mask );

        //Check num threads and mask size are the same
        assert(nthreads==CPU_COUNT(&mask));

        cpu_set_t cpu;
        CPU_ZERO( &cpu );
        sched_getaffinity( 0, sizeof(cpu_set_t), &cpu);
        master_cpu = sched_getcpu();

        if ( blocking_mode == ONE_CPU ) {
            // Remove current cpu from the mask
            CPU_XOR( &mask, &mask, &cpu );
            verbose( VB_MICROLB, "LENDING %d threads", nthreads-1 );
            nthreads = 1;
        } else if ( blocking_mode == BLOCK ) {
            verbose( VB_MICROLB, "LENDING %d threads", nthreads );
            nthreads = 0;
        }

        // Don't set application mask if it is already 1 cpu
        if ( !CPU_EQUAL( &mask, &cpu ) ) {
            set_mask( &cpu );
        }

        // Don't add mask if empty
        if ( CPU_COUNT( &mask ) > 0 ) {
            shmem_cpuinfo__add_mask(&mask);
        }

        add_event( THREADS_USED_EVENT, nthreads );
        //Check num threads and mask size are the same (this is the only case where they don't match)
        //assert(nthreads+1==CPU_COUNT(&cpu));
    }
}

/* Out of Blocking Call - Recover the default number of threads */
void lewi_mask_OutOfBlockingCall(int is_iter) {
    if (enabled) {
        cpu_set_t mask;
        CPU_ZERO( &mask );
        get_mask( &mask );
        //Check num threads and mask size are the same
        //assert(nthreads+1==CPU_COUNT(&mask));

        if (single) {
            verbose(VB_MICROLB, "RECOVERING master thread" );
            shmem_cpuinfo__recover_cpu(master_cpu);
            CPU_SET( master_cpu, &mask );
            nthreads = 1;
        } else {
            verbose(VB_MICROLB, "RECOVERING %d threads", _default_nthreads - nthreads );
            shmem_cpuinfo__recover_some_cpus(&mask, CPUINFO_RECOVER_ALL);
            nthreads = _default_nthreads;
        }
        set_mask( &mask );
        assert(nthreads==CPU_COUNT(&mask));
        add_event( THREADS_USED_EVENT, nthreads );
    }
}

/* Update Resources - Try to acquire foreign threads */
void lewi_mask_UpdateResources( int max_resources ) {
    if (enabled && !single) {
        cpu_set_t mask;
        CPU_ZERO( &mask );
        get_mask( &mask );

        //Check num threads and mask size are the same
        assert(nthreads==CPU_COUNT(&mask));

        int collected = shmem_cpuinfo__collect_mask(&mask, max_resources);

        if ( collected > 0 ) {
            nthreads += collected;
            set_mask( &mask );
            verbose( VB_MICROLB, "ACQUIRING %d threads for a total of %d", collected, nthreads );
            add_event( THREADS_USED_EVENT, nthreads );
        }
        //Check num threads and mask size are the same
        assert(nthreads==CPU_COUNT(&mask));
    }
}

/* Return Claimed CPUs - Return foreign threads that have been claimed by its owner */
void lewi_mask_ReturnClaimedCpus( void ) {
    if (enabled && !single) {
        cpu_set_t mask;
        CPU_ZERO( &mask );
        get_mask( &mask );

        //Check num threads and mask size are the same
        assert(nthreads==CPU_COUNT(&mask));

        int returned = shmem_cpuinfo__return_claimed(&mask);

        if ( returned > 0 ) {
            nthreads -= returned;
            set_mask( &mask );
            verbose( VB_MICROLB, "RETURNING %d threads for a total of %d", returned, nthreads );
            add_event( THREADS_USED_EVENT, nthreads );
        }
        //Check num threads and mask size are the same
        assert(nthreads==CPU_COUNT(&mask));
    }
}

void lewi_mask_ClaimCpus(int cpus) {
    if (enabled && !single) {
        cpu_set_t mask;
        CPU_ZERO( &mask );
        get_mask( &mask );

        if (nthreads<_default_nthreads) {
            //Do not get more cpus than the default ones

            if ((cpus+nthreads)>_default_nthreads) { cpus=_default_nthreads-nthreads; }

            verbose( VB_MICROLB, "Claiming %d cpus", cpus );
            cpu_set_t current_mask;
            CPU_ZERO( &current_mask );

            get_mask( &current_mask );

            //Check num threads and mask size are the same
            assert(nthreads==CPU_COUNT(&current_mask));

            shmem_cpuinfo__recover_some_cpus(&current_mask, cpus);
            set_mask( &current_mask);
            nthreads += cpus;
            assert(nthreads==CPU_COUNT(&current_mask));

            add_event( THREADS_USED_EVENT, nthreads );
        }
    }
}

void lewi_mask_acquireCpu(int cpu) {
    if (enabled && !single) {
        verbose(VB_MICROLB, "AcquireCpu %d", cpu);
        cpu_set_t mask;
        get_mask( &mask );

        if (!CPU_ISSET(cpu, &mask)){

            if (shmem_cpuinfo__acquire_cpu(cpu, 0)) {
                nthreads++;
                CPU_SET(cpu, &mask);
                set_mask( &mask );
                verbose(VB_MICROLB, "New Mask: %s", mu_to_str(&mask));
                add_event( THREADS_USED_EVENT, nthreads );
            }else{
                verbose(VB_MICROLB, "Not legally acquired cpu %d, running anyway",cpu);
            }
        }
    }
}

void lewi_mask_acquireCpus(cpu_set_t* cpus) {
    if (enabled && !single) {
        verbose(VB_MICROLB, "AcquireCpus %s", mu_to_str(cpus));
        cpu_set_t current_mask;
        get_mask( &current_mask );

        bool success = false;
        int cpu;
        for (cpu = 0; cpu < mu_get_system_size(); ++cpu) {
            if (!CPU_ISSET(cpu, &current_mask)
                    && CPU_ISSET(cpu, cpus)){

                if (shmem_cpuinfo__acquire_cpu(cpu, 0)) {
                    nthreads++;
                    CPU_SET(cpu, &current_mask);
                    success = true;
                } else {
                    verbose(VB_MICROLB, "Not legally acquired cpu %d, running anyway",cpu);
                }
            }
        }
        if (success) {
            set_mask( &current_mask );
            verbose(VB_MICROLB, "New Mask: %s", mu_to_str(&current_mask));
            add_event( THREADS_USED_EVENT, nthreads );
        }
    }
}

void lewi_mask_resetDLB( void ) {
    if (enabled && !single) {
        verbose(VB_MICROLB, "ResetDLB");
        cpu_set_t current_mask;
        CPU_ZERO( &current_mask );
        get_mask( &current_mask );
        nthreads = shmem_cpuinfo__reset_default_cpus(&current_mask);
        set_mask( &current_mask);
        verbose(VB_MICROLB, "New Mask: %s", mu_to_str(&current_mask));
    }
}

void lewi_mask_disableDLB(void) {
    lewi_mask_resetDLB();
    enabled = 0;
}

void lewi_mask_enableDLB(void) {
    single = 0;
    enabled = 1;
}

void lewi_mask_single(void) {
    lewi_mask_IntoBlockingCall(0, ONE_CPU);
    single = 1;
}

void lewi_mask_parallel(void) {
    lewi_mask_OutOfBlockingCall(0);
    single = 0;
}
