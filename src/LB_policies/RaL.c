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
#include <string.h>
#include <stdio.h>

#include "LB_numThreads/numThreads.h"
#include "LB_comm/shmem_bitset.h"
#include "LB_core/spd.h"
#include "support/debug.h"
#include "support/tracing.h"
#include "support/mask_utils.h"
#include "support/mytime.h"


static int initial_nthreads;
static int nthreads;
static int max_cpus;

static int iterNum;
static struct timespec initIter;
static struct timespec initComp;
//static struct timespec compIter;
static double iter_cpu;
static double previous_iter;

/******* Main Functions - LeWI Mask Balancing Policy ********/

void RaL_Init(const cpu_set_t *process_mask) {
    verbose( VB_MICROLB, "LeWI Mask Balancing Init" );

    initial_nthreads = CPU_COUNT(process_mask);
    nthreads = initial_nthreads;
    max_cpus = initial_nthreads;

    //Initialize iterative info
//   reset(&compIter);
    iterNum=0;
    iter_cpu=0;

    //Initialize shared memory
    shmem_mask.init( process_mask );
}

void RaL_Finish( void ) {
    set_mask( &global_spd.pm, shmem_mask.recover_defmask() );
    shmem_mask.finalize();
}

void RaL_IntoCommunication( void ) {}

void RaL_OutOfCommunication( void ) {}

/* Into Blocking Call - Lend the maximum number of threads */
void RaL_IntoBlockingCall(int is_iter, int blocking_mode) {
    struct timespec aux;
    if (clock_gettime(CLOCK_REALTIME, &aux)<0) {
        fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
    }

    diff_time(initComp, aux, &aux);
//   add_time(compIter, aux, &compIter);
    iter_cpu+=to_secs(aux) * nthreads;

    cpu_set_t mask;
    memcpy(&mask, &global_spd.active_mask, sizeof(cpu_set_t));

    cpu_set_t cpu;
    CPU_ZERO( &cpu );
    sched_getaffinity( 0, sizeof(cpu_set_t), &cpu);

    if ( blocking_mode == ONE_CPU ) {
        // Remove current cpu from the mask
        CPU_XOR( &mask, &mask, &cpu );
        verbose( VB_MICROLB, "LENDING %d threads", nthreads-1 );
        nthreads = 1;
    } else if ( blocking_mode == BLOCK ) {
        verbose( VB_MICROLB, "LENDING %d threads", nthreads );
        nthreads = 0;
    }

    set_mask( &global_spd.pm, &cpu );
    shmem_mask.add_mask( &mask );
    add_event( THREADS_USED_EVENT, nthreads );
}

/* Out of Blocking Call - Recover the default number of threads */
void RaL_OutOfBlockingCall(int is_iter ) {

    if (clock_gettime(CLOCK_REALTIME, &initComp)<0) {
        fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
    }

    if (is_iter!=0) {
        struct timespec aux;

        if(iterNum!=0) {
            double iter_duration;
            //Finishing Iteration
            //aux is the duration of the last iteration
            diff_time(initIter, initComp, &aux);
            iter_duration=to_secs(aux);
            //If the iteration has been longer than the last maybe something went wrong, recover one cpu
            verbose( VB_MICROLB, "Iter %d:  %.4f (%.4f) max_cpus:%d", iterNum, to_secs(aux), iter_cpu, max_cpus);
            if ((iter_duration>=previous_iter) && (max_cpus<initial_nthreads)) { max_cpus++; }
            else {
                iter_cpu=iter_cpu/(max_cpus-1);
                //If we can do the same computations with one cpu less in less time than the iteration time, release a cpu "forever"
                //but only if we still have one cpu
                if(iter_cpu<iter_duration && max_cpus>0) {
                    max_cpus--;
//          fprintf(stderr, "[%d] with one cpu less: %.4f - %.4f (iter): %d\n", _mpi_rank, iter_cpu, to_secs(aux), iter_cpu<to_secs(aux) );
                }
            }
            previous_iter=iter_duration;
        }
        //Init Iteration
        add_event(ITERATION_EVENT, iterNum);
        iterNum++;
        initIter=initComp;
        iter_cpu=0;
    }

    verbose( VB_MICROLB, "RECOVERING %d threads", max_cpus - nthreads );
    cpu_set_t mask;
    CPU_ZERO( &mask );
    shmem_mask.recover_some_defcpus( &mask, max_cpus );
    set_mask( &global_spd.pm, &mask );
    nthreads = max_cpus;
//printf(stderr, "[%d] Using %d threads\n", _mpi_rank, nthreads );

    add_event( THREADS_USED_EVENT, nthreads );

}

/* Update Resources - Try to acquire foreign threads */
void RaL_UpdateResources( int max_resources ) {
    cpu_set_t mask;
    memcpy(&mask, &global_spd.active_mask, sizeof(cpu_set_t));

    if( max_cpus==initial_nthreads || ((iter_cpu/max_cpus)>(previous_iter/2))) {
        //fprintf(stderr, "[%d] max_cpus %d < default_threads %d = %d (max_resources=%d)\n", _mpi_rank, max_cpus, initial_nthreads, max_cpus<initial_nthreads, max_resources );

        int new_threads = shmem_mask.collect_mask( &mask, max_resources );
        //printf(stderr, "[%d] dirty:%d new_threads:%d\n", _mpi_rank, dirty, new_threads);

        if ( new_threads > 0 ) {
            struct timespec aux;
            if (clock_gettime(CLOCK_REALTIME, &aux)<0) {
                fprintf(stderr, "DLB ERROR: clock_gettime failed\n");
            }

            diff_time(initComp, aux, &initComp);
            iter_cpu+=to_secs(initComp) * nthreads;

            initComp=aux;

            nthreads += new_threads;
            set_mask( &global_spd.pm, &mask );
            verbose( VB_MICROLB, "ACQUIRING %d threads for a total of %d", new_threads, nthreads );
            add_event( THREADS_USED_EVENT, nthreads );
        }
    }
}

/* Return Claimed CPUs - Return foreign threads that have been claimed by its owner */
void RaL_ReturnClaimedCpus( void ) {
    cpu_set_t mask;
    memcpy(&mask, &global_spd.active_mask, sizeof(cpu_set_t));

    int returned = shmem_mask.return_claimed( &mask );

    if ( returned > 0 ) {
        nthreads -= returned;
        set_mask( &global_spd.pm, &mask );
        verbose( VB_MICROLB, "RETURNING %d threads for a total of %d", returned, nthreads );
        add_event( THREADS_USED_EVENT, nthreads );
    }
}
