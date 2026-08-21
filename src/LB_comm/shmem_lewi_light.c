/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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

#include "LB_comm/shmem_lewi_light.h"

#include "LB_comm/shmem.h"
#include "support/tracing.h"
#include "support/debug.h"
#include "support/types.h"

#include <stdlib.h>

static int defaultCPUS;
static int greedy;

struct shdata {
    int   idleCpus;
    int   attached_nprocs;
};

static struct shdata *shdata = NULL;
static shmem_handler_t *shm_handler = NULL;
static const char *shmem_name = "lewi";
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int subprocesses_attached = 0;

static void cleanup_shmem(void *shdata_ptr, int pid) {
    struct shdata *shared_data = shdata_ptr;
    __sync_fetch_and_sub(&shared_data->attached_nprocs, 1);
}

static bool is_shmem_empty(void) {
    return shdata && shdata->attached_nprocs == 0;
}

static void open_shmem(const char *shmem_key) {
    pthread_mutex_lock(&mutex);
    {
        if (shm_handler == NULL) {
            shm_handler = shmem_init((void**)&shdata,
                    &(const shmem_props_t) {
                        .size = sizeof(struct shdata),
                        .name = shmem_name,
                        .key = shmem_key,
                        .version = SHMEM_VERSION_IGNORE,
                        .cleanup_fn = cleanup_shmem,
                    });
            subprocesses_attached = 1;
        } else {
            ++subprocesses_attached;
        }
    }
    pthread_mutex_unlock(&mutex);
}

void shmem_lewi_light__init(int def_cpus, int is_greedy, const char *shmem_key) {
    verbose(VB_SHMEM, "Initializing shmem_lewi_light");
    defaultCPUS = def_cpus;
    greedy = is_greedy;

    // Shared memory creation
    open_shmem(shmem_key);

    if (__sync_fetch_and_add(&shdata->attached_nprocs, 1) == 0) {
        // Initialize shared memory if this is the 1st process attached
        verbose(VB_SHMEM, "setting values to the shared mem");

        /* idleCPUS */
        shdata->idleCpus = 0;
        add_event(IDLE_CPUS_EVENT, 0);

        verbose(VB_SHMEM, "Finished setting values to the shared mem");
    }
}

static void close_shmem(void) {
    pthread_mutex_lock(&mutex);
    {
        if (--subprocesses_attached == 0) {
            shmem_finalize(shm_handler, is_shmem_empty);
            shm_handler = NULL;
            shdata = NULL;
        }
    }
    pthread_mutex_unlock(&mutex);
}

void shmem_lewi_light__finalize(void) {
    if (shm_handler) {
        __sync_fetch_and_sub(&shdata->attached_nprocs, 1);
        close_shmem();
    }
}

int shmem_lewi_light__release_cpus(int cpus) {
    verbose(VB_SHMEM, "Releasing CPUS...");

    __sync_fetch_and_add (&(shdata->idleCpus), cpus);
    add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

    verbose(VB_SHMEM, "DONE Releasing CPUS (idle %d)", shdata->idleCpus);

    return 0;
}

int shmem_lewi_light__acquire_cpus(int current_cpus) {
    verbose(VB_SHMEM, "Acquiring CPUS...");
    int cpus = defaultCPUS-current_cpus;

//I don't care if there aren't enough cpus

    if((__sync_sub_and_fetch (&(shdata->idleCpus), cpus)>0) && greedy) {
        cpus+=__sync_val_compare_and_swap(&(shdata->idleCpus), shdata->idleCpus, 0);
    }
    add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

    verbose(VB_SHMEM, "Using %d CPUS... %d Idle", cpus, shdata->idleCpus);

    return cpus+current_cpus;
}

int shmem_lewi_light__check_idle_cpus(int my_cpus, int max_resources) {
    verbose(VB_SHMEM, "Checking idle CPUS... %d", shdata->idleCpus);
    int cpus;
    int aux;
//WARNING//
    //if more CPUS than the availables are used release some
    if ((shdata->idleCpus < 0) && (my_cpus>defaultCPUS) ) {
        aux=shdata->idleCpus;
        cpus=min_int(abs(aux), my_cpus-defaultCPUS);
        if(__sync_bool_compare_and_swap(&(shdata->idleCpus), aux, aux+cpus)) {
            my_cpus-=cpus;
        }

        //if there are idle CPUS use them
    } else if( shdata->idleCpus > 0) {
        aux=shdata->idleCpus;
        if(aux>max_resources) { aux=max_resources; }

        if(__sync_bool_compare_and_swap(&(shdata->idleCpus), shdata->idleCpus, shdata->idleCpus-aux)) {
            my_cpus+=aux;
        }
    }
    add_event(IDLE_CPUS_EVENT, shdata->idleCpus);

    verbose(VB_SHMEM, "Using %d CPUS... %d Idle", my_cpus, shdata->idleCpus);
    return my_cpus;
}

void shmem_lewi_light__atfork_prepare(void) {
    pthread_mutex_lock(&mutex);
}

void shmem_lewi_light__atfork_parent(void) {
    pthread_mutex_unlock(&mutex);
}

void shmem_lewi_light__atfork_child(void) {

    pthread_mutex_init(&mutex, NULL);

    if (shm_handler != NULL) {
        shmem_detach_after_fork(shm_handler);
        shdata = NULL;
        shm_handler = NULL;
    }
}
