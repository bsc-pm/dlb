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

/*<testinfo>
    test_generator="gens/basic-generator"
</testinfo>*/

#include "assert_noshm.h"

#include "apis/dlb.h"
#include "apis/dlb_sp.h"
#include "support/mask_utils.h"

#include <pthread.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Simulate execution with NSUBPROCS subprocesses
 * and stress test proc requests and finalization */
enum { NUM_TESTS = 100 };
enum { SYS_SIZE = 16 };
enum { NSUBPROCS = 4 };
static cpu_set_t sp_mask[NSUBPROCS];
static dlb_handler_t handlers[NSUBPROCS];
static int petitions[NSUBPROCS];
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

#ifndef VERBOSE
#define VERBOSE 0
#endif
static void print_vb(const char *fmt, ...) {
    if (VERBOSE) {
        va_list list;
        va_start(list, fmt);
        vprintf(fmt, list);
        va_end(list);
    }
}

static void cb_enable_cpu(int cpuid, void *arg) {
    int thid = *((int*)arg);
    cpu_set_t *mask = &sp_mask[thid];
    print_vb("Sp %d enabling CPU %d\n", thid, cpuid);

    pthread_mutex_lock(&mutex);
    CPU_SET(cpuid, mask);

    /* Assume the thread arrives late, and it's not needed unless it's the last request */
    if (petitions[thid] > 0) {
        --petitions[thid];
        pthread_mutex_unlock(&mutex);

        print_vb("Sp %d lending CPU %d (petitions: %d)\n", thid, cpuid, petitions[thid]);
        CPU_CLR(cpuid, mask);
        DLB_LendCpu_sp(handlers[thid], cpuid);
    } else {
        pthread_mutex_unlock(&mutex);
    }
}

static void* thread_start(void *arg) {
    int thid = *((int*)arg);
    cpu_set_t *mask = &sp_mask[thid];
    print_vb("Executing sp %d with mask %s\n", thid, mu_to_str(mask));

    /* Initialization */
    handlers[thid] = DLB_Init_sp(0, mask, "--lewi --mode=async --quiet");
    assert( handlers[thid] != NULL );
    assert( DLB_CallbackSet_sp(handlers[thid], dlb_callback_enable_cpu,
                (dlb_callback_t)cb_enable_cpu, arg) == DLB_SUCCESS);

    /* Simulate a high resource demand: make as many petitions as the system size */
    int i;
    petitions[thid] = 0;
    for (i=CPU_COUNT(mask); i<SYS_SIZE; ++i) {
        pthread_mutex_lock(&mutex);
        {
            int err = DLB_AcquireCpus_sp(handlers[thid], 1);
            if (err < 0) {
                printf("DLB_AcquireCpus_sp from Sp %d returned %s\n", thid, DLB_Strerror(err));
                abort();
            }
            ++petitions[thid];
        }
        pthread_mutex_unlock(&mutex);
    }
    print_vb("Sp %d finished compute phase with %d petitions\n", thid, petitions[thid]);

    /* The workload is over: lend all the resources */
    for (i=0; i<SYS_SIZE; ++i) {
        pthread_mutex_lock(&mutex);
        {
            if (CPU_ISSET(i, mask)) {
                print_vb("Sp %d lending CPU %d\n", thid, i);
                CPU_CLR(i, mask);
                DLB_LendCpu_sp(handlers[thid], i);
            }
        }
        pthread_mutex_unlock(&mutex);
    }

    /* wait until all petitions have been requested */
    while (petitions[thid] != 0) {usleep(100);}

    print_vb("Sp %d finished\n", thid);

    /* Finalization */
    assert( DLB_Finalize_sp(handlers[thid]) == DLB_SUCCESS );

    return NULL;
}

int main( int argc, char **argv ) {
    mu_init();
    mu_testing_set_sys_size(SYS_SIZE);

    assert( SYS_SIZE % NSUBPROCS == 0 );

    int i, j;
    int cpus_per_subproc = SYS_SIZE / NSUBPROCS;
    cpu_set_t original_masks[NSUBPROCS];
    for (i=0; i<NSUBPROCS; ++i) {
        CPU_ZERO(&original_masks[i]);
        for (j=cpus_per_subproc*i; j<cpus_per_subproc*(i+1); ++j) {
            CPU_SET(j, &original_masks[i]);
        }
    }

    for (i=0; i<NUM_TESTS; ++i) {
        memcpy(sp_mask, original_masks, sizeof(sp_mask));

        pthread_t threads[NSUBPROCS];
        int thread_ids[NSUBPROCS];
        for (j=0; j<NSUBPROCS; ++j) {
            thread_ids[j] = j;
            pthread_create(&threads[j], NULL, thread_start, &thread_ids[j]);
        }

        for (j=0; j<NSUBPROCS; ++j) {
            pthread_join(threads[j], NULL);
        }

        if ((i+1) % (NUM_TESTS/10) == 0) {
            printf("Iteration %d completed\n", i+1);
        }
    }

    return 0;
}
