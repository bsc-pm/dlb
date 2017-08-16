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

#include "autonomous_lewi_mask.h"

#include "LB_numThreads/numThreads.h"
#include "LB_comm/shmem_cpuinfo.h"
#include "LB_core/spd.h"
#include "apis/dlb_errors.h"
#include "support/debug.h"
#include "support/tracing.h"
#include "support/options.h"
#include "support/mask_utils.h"

#include <sched.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <assert.h>

static int nthreads;
static pthread_mutex_t mutex;
static int enabled=0;
static int single=0;

int auto_lewi_mask_Init(const subprocess_descriptor_t *spd) {
    verbose(VB_MICROLB, "Auto LeWI Mask Balancing Init");

    pthread_mutex_init(&mutex, NULL);
    nthreads = CPU_COUNT(&spd->process_mask);
    enabled=1;

    add_event(THREADS_USED_EVENT, nthreads);
    return DLB_SUCCESS;
}

int auto_lewi_mask_Finish(const subprocess_descriptor_t *spd) {
    //cpu_set_t mask;
    //CPU_ZERO(&mask);
    pthread_mutex_lock(&mutex);
    shmem_cpuinfo__recover_all(spd->id, NULL);
    //if (CPU_COUNT(&mask)>0) add_mask(&spd->pm, &mask);
    enabled=0;
    pthread_mutex_unlock(&mutex);
    pthread_mutex_destroy(&mutex);
    return DLB_SUCCESS;
}

int auto_lewi_mask_EnableDLB(const subprocess_descriptor_t *spd) {
    pthread_mutex_lock ( &mutex);
    single=0;
    enabled=1;
    verbose(VB_MICROLB, "Enabling DLB");
    pthread_mutex_unlock (&mutex);
    return DLB_SUCCESS;
}

int auto_lewi_mask_DisableDLB(const subprocess_descriptor_t *spd) {
    pthread_mutex_lock(&mutex);
    if (enabled){
        verbose(VB_MICROLB, "Disabling DLB");
        cpu_set_t current_mask;
        memcpy(&current_mask, &spd->active_mask, sizeof(cpu_set_t));
        nthreads=shmem_cpuinfo__reset_default_cpus(spd->id, &current_mask);
        set_mask(&spd->pm, &current_mask);
        enabled=0;
        verbose(VB_MICROLB, "New Mask: %s", mu_to_str(&current_mask));
        add_event(THREADS_USED_EVENT, nthreads);
    }
    pthread_mutex_unlock(&mutex);
    return DLB_SUCCESS;
}

int auto_lewi_mask_IntoCommunication(const subprocess_descriptor_t *spd) { return DLB_SUCCESS; }

int auto_lewi_mask_OutOfCommunication(const subprocess_descriptor_t *spd) { return DLB_SUCCESS; }

/* Into Blocking Call - Lend the maximum number of threads */
int auto_lewi_mask_IntoBlockingCall(const subprocess_descriptor_t *spd) {

    if ( spd->options.mpi_lend_mode == BLOCK ) {
        cpu_set_t cpu;
        CPU_ZERO( &cpu );
        sched_getaffinity( 0, sizeof(cpu_set_t), &cpu);

        int i=0;

        while (i< mu_get_system_size()){
            if (CPU_ISSET(i, &cpu)) break; 
            i++;
        }
        if (i!=mu_get_system_size()){
            verbose(VB_MICROLB, "IntoBlockingCall: lending cpu: %d",  i);

            pthread_mutex_lock(&mutex);
            if (enabled) {
                cpu_set_t current_mask;
                memcpy(&current_mask, &spd->active_mask, sizeof(cpu_set_t));
                if (CPU_ISSET(i, &current_mask)) {
                    shmem_cpuinfo__add_cpu(spd->id, i, NULL);
                    nthreads--;
                    add_event(THREADS_USED_EVENT, nthreads);
                }
            }
            pthread_mutex_unlock(&mutex);
        }
    }
    return DLB_SUCCESS;
}

/* Out of Blocking Call - Recover the default number of threads */
int auto_lewi_mask_OutOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter) {

    if ( spd->options.mpi_lend_mode == BLOCK ) {
        cpu_set_t mask;
        CPU_ZERO(&mask);
        sched_getaffinity(0, sizeof(cpu_set_t), &mask);

        int my_cpu=0;

        while (my_cpu < mu_get_system_size()){
            if (CPU_ISSET(my_cpu, &mask)) break;
            my_cpu++;
        }

        if (my_cpu!=mu_get_system_size()){
            verbose(VB_MICROLB, "OutOfBlockingCall: recovering cpu: %d", my_cpu);
            CPU_ZERO( &mask );

            pthread_mutex_lock(&mutex);
            if (enabled && !single){
                memcpy(&mask, &spd->active_mask, sizeof(cpu_set_t));

            // FIXME: steal force is disabled
                if (shmem_cpuinfo__borrow_cpu(spd->id, my_cpu, NULL)) {
                    nthreads++;
                    add_event(THREADS_USED_EVENT, nthreads);
                } else {
                    CPU_CLR(my_cpu, &mask);
                    set_mask(&spd->pm, &mask);
                    verbose(VB_MICROLB, "Can't recover cpu, remove from Mask, new mask: %s",
                            mu_to_str(&mask));
                }
            }
            pthread_mutex_unlock(&mutex);
        }
    }
    return DLB_SUCCESS;
}

int auto_lewi_mask_Lend(const subprocess_descriptor_t *spd) {
    auto_lewi_mask_IntoBlockingCall(spd);
    return DLB_SUCCESS;
}

int auto_lewi_mask_LendCpu(const subprocess_descriptor_t *spd, int cpuid) {
    int error = DLB_ERR_PERM;
    pthread_mutex_lock(&mutex);
    if (enabled) {
        verbose(VB_MICROLB, "Release cpu %d", cpuid);
        cpu_set_t current_mask;
        memcpy(&current_mask, &spd->active_mask, sizeof(cpu_set_t));

        if (CPU_ISSET(cpuid, &current_mask)) {
            shmem_cpuinfo__add_cpu(spd->id, cpuid, NULL);
            CPU_CLR( cpuid, &current_mask );
            set_mask(&spd->pm, &current_mask);
            verbose(VB_MICROLB, "New Mask: %s", mu_to_str(&current_mask));
            nthreads--;
            add_event( THREADS_USED_EVENT, nthreads );
            error = DLB_SUCCESS;
        }

    } else {
        error = DLB_ERR_DISBLD;
    }
    pthread_mutex_unlock(&mutex);
    return error;
}

int auto_lewi_mask_ReclaimCpus(const subprocess_descriptor_t *spd, int ncpus) {
    int error = DLB_ERR_PERM;
    pthread_mutex_lock(&mutex);
    if (enabled && !single) {
        verbose(VB_MICROLB, "ClaimCpus max: %d", ncpus);
        // FIXME
        //if ((ncpus+nthreads)>_default_nthreads) { ncpus=_default_nthreads-nthreads; }

        cpu_set_t current_mask, mask;
        memcpy(&current_mask, &spd->active_mask, sizeof(cpu_set_t));
        memcpy(&mask, &spd->active_mask, sizeof(cpu_set_t));

        shmem_cpuinfo__recover_cpus(spd->id, ncpus, NULL);
        if (!CPU_EQUAL(&current_mask, &mask)) {
            set_mask(&spd->pm, &mask);
            nthreads = CPU_COUNT(&mask);
            add_event(THREADS_USED_EVENT, nthreads);
            verbose(VB_MICROLB, "New Mask: %s", mu_to_str(&mask));
            error = DLB_SUCCESS;
        }
    } else {
        error = DLB_ERR_DISBLD;
    }
    pthread_mutex_unlock(&mutex);
    return error;
}

int auto_lewi_mask_AcquireCpu(const subprocess_descriptor_t *spd, int cpuid) {
    int error = DLB_ERR_PERM;
    pthread_mutex_lock(&mutex);
    if (enabled && !single){
        if (shmem_cpuinfo__borrow_cpu(spd->id, cpuid, NULL)) {
            nthreads++;
            enable_cpu(&spd->pm, cpuid);
            verbose(VB_MICROLB, "Acquiring CPU %d", cpuid);
            add_event(THREADS_USED_EVENT, nthreads);
            error = DLB_SUCCESS;
        }else{
            verbose(VB_MICROLB, "Acquire CPU %d failed, running anyway",cpuid);
        }
    } else {
        error = DLB_ERR_DISBLD;
    }
    pthread_mutex_unlock(&mutex);
    return error;
}

int auto_lewi_mask_AcquireCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask) {
    int error = DLB_ERR_PERM;
    pthread_mutex_lock(&mutex);
    if (enabled && !single) {
        verbose(VB_MICROLB, "AcquireCpu %s", mu_to_str(mask));
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
            set_mask(&spd->pm, &current_mask);
            verbose(VB_MICROLB, "New Mask: %s", mu_to_str(&current_mask));
            add_event( THREADS_USED_EVENT, nthreads );
            error = DLB_SUCCESS;
        }
    } else {
        error = DLB_ERR_DISBLD;
    }
    pthread_mutex_unlock(&mutex);
    return error;
}

/* Try to acquire foreign threads */
int auto_lewi_mask_BorrowCpus(const subprocess_descriptor_t *spd, int ncpus) {
    int error = DLB_ERR_PERM;
    pthread_mutex_lock(&mutex);
    if (enabled && !single) {
        verbose(VB_MICROLB, "UpdateResources");
        cpu_set_t mask;
        memcpy(&mask, &spd->active_mask, sizeof(cpu_set_t));

        int collected = shmem_cpuinfo__collect_mask(spd->id, &mask, ncpus, spd->options.priority);

        if ( collected > 0 ) {
            nthreads += collected;
            set_mask(&spd->pm, &mask);
            verbose(VB_MICROLB, "Acquiring %d cpus, new Mask: %s", collected, mu_to_str(&mask));
            add_event( THREADS_USED_EVENT, nthreads );
            error = DLB_SUCCESS;
        }
    } else {
        error = DLB_ERR_DISBLD;
    }
    pthread_mutex_unlock(&mutex);
    return error;
}

/* Return Claimed CPUs - Return foreign threads that have been claimed by its owner */
/*DEPRECATED
 In the auto_LeWI_mask, every thread will detect as soon as posible
 that its cpu has been claimed.
 We do not need a generic function that checks all the cpus
 */

/*void auto_lewi_mask_ReturnClaimedCpus( void ) {
    cpu_set_t mask;
    CPU_ZERO( &mask );

    pthread_mutex_lock(&mutex);
    if (enabled) {
        get_mask( &mask );
        int returned = shmem_cpuinfo__return_claimed(&mask);

        if ( returned > 0 ) {
            nthreads -= returned;
            set_mask( &mask );
            verbose(VB_MICROLB, "Mask: %s", mu_to_str(&mask));
            add_event( THREADS_USED_EVENT, nthreads );
        }
    }
    pthread_mutex_unlock(&mutex);
}*/

int auto_lewi_mask_ReturnCpu(const subprocess_descriptor_t *spd, int cpuid) {
    int error = DLB_ERR_PERM;
    if (shmem_cpuinfo__is_cpu_claimed(spd->id, cpuid)) {
        cpu_set_t release_mask;
        CPU_ZERO( &release_mask );
        CPU_SET(cpuid, &release_mask);

        pthread_mutex_lock(&mutex);
        if (enabled) {
            cpu_set_t mask;
            memcpy(&mask, &spd->active_mask, sizeof(cpu_set_t));
            verbose(VB_MICROLB, "ReturnClaimedCpu %d", cpuid);
            if ( CPU_ISSET( cpuid, &mask)) {
                int returned = shmem_cpuinfo__return_claimed(spd->id, &release_mask);

                if ( returned > 0 ) {
                    nthreads -= returned;
                    CPU_CLR(cpuid, &mask);
                    set_mask(&spd->pm, &mask);
                    verbose(VB_MICROLB, "New Mask: %s", mu_to_str(&mask));
                    add_event( THREADS_USED_EVENT, nthreads );
                    error = DLB_SUCCESS;
                }
            }
        }
        pthread_mutex_unlock(&mutex);
    }
    return error;
}

int auto_lewi_mask_CheckCpuAvailability(const subprocess_descriptor_t *spd, int cpuid) {
    return shmem_cpuinfo__is_cpu_borrowed(spd->id, cpuid);
}
