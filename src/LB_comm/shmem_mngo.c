/*********************************************************************************/
/*  Copyright 2009-2023 Barcelona Supercomputing Center                          */
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

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_mngo.h"
#include "LB_comm/shmem_procinfo.h"
#include "support/dlb_common.h"
#include "support/types.h"
#include <pthread.h>
#include <sched.h>
#include <stddef.h>

#ifdef MPI_LIB
#include "mpi/mpi_core.h"
#endif

#include "support/debug.h"
#include "support/mask_utils.h"

#include "apis/dlb_errors.h"

#include <stdio.h>
#include <string.h>

static int _mngo_test_barrier_participants = -1;

/**
 * The struct containing the message if any of a manager.
 */
typedef struct {
    pid_t pid;
    int rank;

    // For the ACTIONS all2all
    mngo_actions_t actions;

    // For the DROM all2all's
    int drom_core_change;
    int dcores;
    int ncores;
} mngo_manager_info_t;

/**
 * The data structure of the MNGO shared memory.
 */
typedef struct {
    int attached;
    pthread_barrier_t barrier;
    pthread_mutex_t manager_lock;
    pthread_cond_t manager_cond;
    bool stop;
    pthread_mutex_t stop_lock;
    cpu_set_t available_cpus;
    mngo_manager_info_t manager_info[0];
} shdata_t ;

/**
 * The pointer to the data structure used by MNGO for the communication among
 * the different manager threads.
 */
static shdata_t *shdata = NULL;

/**
 * The pointer to the shm handler to interact with the shmem interface.
 */
static shmem_handler_t *shm_handler = NULL;

/**
 * The maximum of sub processes that can be atached to the shared memory.
 */
static size_t max_attached;

/**
 * The amount of sub processes atached to the shared memory.
 */
static size_t subprocesses_attached;

/**
 * The shared memory name. Used to identify shared memory files for the MNGo module.
 */
static const char *shmem_name = "mngo";

/**
 * Process wide pthread mutex.
 */
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

static void close_shmem(void) {
    pthread_mutex_lock(&mutex);
    subprocesses_attached--;
    if (subprocesses_attached == 0) {
        shmem_finalize(shm_handler, NULL /* do not check if empty */);
    }
    pthread_mutex_unlock(&mutex);
}

static int shmem_mngo_size(void) {
    return sizeof(shdata_t) + sizeof(mngo_manager_info_t) * max_attached;
}

static void shmem_mngo_barrier_init(int participants) {
    pthread_barrierattr_t attr;
    pthread_barrierattr_init(&attr);
    pthread_barrierattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_barrier_init(&shdata->barrier, &attr, participants);
    pthread_barrierattr_destroy(&attr);
}

static void shmem_mngo_lock_init(pthread_mutex_t * lock) {
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(lock, &attr);
    pthread_mutexattr_destroy(&attr);
}

static void shmem_mngo_manager_cond_init(void) {
    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&shdata->manager_cond, &attr);
    pthread_condattr_destroy(&attr);
}


/* This function may be called from shmem_init to cleanup pid */
static void cleanup_shmem(void *shdata_ptr, int pid) {
    bool shmem_empty = true;
    shdata_t *shared_data = shdata_ptr;
    size_t id;
    for (id = 0; id < max_attached; id++) {
        if (shared_data->manager_info[id].pid == pid) {
            memset(&shdata->manager_info[id], 0, sizeof(mngo_manager_info_t));
            break;
        } else if (shared_data->manager_info[id].pid > 0) {
            shmem_empty = false;
        }
    }

    /* If there are no registered processes, make sure shmem is reset */
    if (shmem_empty) {
        memset(shared_data, 0, shmem_mngo_size());
    }
}

int shmem_mngo_init(const char *shmem_key, pid_t pid, int rank, size_t *mid) {

    int error = DLB_ERR_UNKNOWN;

#ifdef MPI_LIB
    int barrier_participants  = _mpis_per_node;
#else
    int barrier_participants = 1;
#endif

    if (unlikely(_mngo_test_barrier_participants > -1)) {
        barrier_participants = _mngo_test_barrier_participants;
    }

    pthread_mutex_lock(&mutex);
    {
      if (shm_handler == NULL) {
        max_attached = mu_get_system_size();
        shm_handler = shmem_init((void**)&shdata,
                &(const shmem_props_t) {
                    .size = shmem_mngo_size(),
                    .name = shmem_name,
                    .key = shmem_key,
                    .version = SHMEM_VERSION_IGNORE,
                    .cleanup_fn = cleanup_shmem,
                });
        subprocesses_attached = 1;
      } else {
        subprocesses_attached++;
      }
    }
    pthread_mutex_unlock(&mutex);

    size_t id;
    shmem_lock(shm_handler);
    {
        /* If it is the first process to access this the whole shm will be 0 */
        for (id = 0; id < max_attached; id++) {
            if (! shdata->manager_info[id].pid) {
                shdata->manager_info[id].pid = pid;
                shdata->manager_info[id].rank = rank;
                error = DLB_SUCCESS;
                break;
            }
        }

        if (id == 0) {
            shmem_mngo_barrier_init(barrier_participants);
            shmem_mngo_lock_init(&shdata->manager_lock);
            shmem_mngo_lock_init(&shdata->stop_lock);
            shmem_mngo_manager_cond_init();
        }

        shdata->attached++;
    }
    shmem_unlock(shm_handler);

    if (id == max_attached) {
        error = DLB_ERR_NOSHMEM;
    }
    
    if (error != DLB_SUCCESS) {
      close_shmem();
      warn_error(error);
    }
    
    *mid = id;
    
    return error;
}

static void shmem_mngo_ext__init(const char *shmem_key) {
    max_attached = mu_get_system_size();
    shm_handler = shmem_init((void**)&shdata,
            &(const shmem_props_t) {
                .size = shmem_mngo_size(),
                .name = shmem_name,
                .key = shmem_key,
                .version = SHMEM_VERSION_IGNORE,
            });
}

int shmem_mngo_fini(size_t mid) {
    bool was_active;
    shmem_lock(shm_handler);
    {
        was_active = shdata->manager_info[mid].pid != 0;
        shdata->manager_info[mid].pid = 0;
    }
    shmem_unlock(shm_handler);

    if (was_active) {
        close_shmem();
    }

    return DLB_SUCCESS;
}

void shmem_mngo_fini_sync(void) {
    shmem_lock(shm_handler);
    {
        shdata->attached--;
    }
    shmem_unlock(shm_handler);
    while (1) {
        int num_attached;
        shmem_mngo_barrier_wait();
        shmem_lock(shm_handler);
        {
            num_attached = shdata->attached;
        }
        shmem_unlock(shm_handler);
        if (num_attached == 0) break;
    }
}

static int shmem_mngo_ext__finalize(void) {
    // Protect double finalization
    if (shm_handler == NULL) {
        return DLB_ERR_NOSHMEM;
    }

    // Shared memory destruction
    shmem_finalize(shm_handler, NULL /* do not check if empty */);
    shm_handler = NULL;
    shdata = NULL;

    return DLB_SUCCESS;
}

static int assert_manager_active(const size_t mid) {

    int error = DLB_SUCCESS;

    if (mid > max_attached || !shdata->manager_info[mid].pid) {
        error = DLB_ERR_UNKNOWN;
    }

    return error;
}

void shmem_mngo_barrier_wait(void) {
    pthread_barrier_wait(&shdata->barrier);
}

// not thread-safe, this function should be guarded with a shdata->stop_lock
void shmem_mngo_set_stop_flag(bool flag) {
    shmem_lock(shm_handler);
    shdata->stop = flag;
    shmem_unlock(shm_handler);
}

bool shmem_mngo_check_stop_flag(void) {
    shmem_lock(shm_handler);
    bool stop = shdata->stop;
    shmem_unlock(shm_handler);

    return stop;
}

void shmem_mngo_manager_wake(void) {
    // Aquire the lock.
    pthread_mutex_lock(&shdata->manager_lock);

    // Wake up the manager thread via the pthread cond.
    pthread_cond_signal(&shdata->manager_cond);

    // Imediatly release the lock.
    pthread_mutex_unlock(&shdata->manager_lock);
}

void shmem_mngo_manager_wake_finalize(void) {
    // The only moment we can aquire both locks is when shmem_mngo_manager_wait
    // is wating in the `pthread_cond_wait`
    pthread_mutex_lock(&shdata->stop_lock);
    pthread_mutex_lock(&shdata->manager_lock);

    // Set the finalize flag
    shmem_mngo_set_stop_flag(true);

    // Wake up the manager thread via the pthread cond.
    pthread_cond_broadcast(&shdata->manager_cond);

    // Imediatly release the lock.
    pthread_mutex_unlock(&shdata->manager_lock);
    pthread_mutex_unlock(&shdata->stop_lock);
}

void shmem_mngo_manager_wait(const struct timespec *abstime) {

    // Release the lock while I am asleep
    pthread_mutex_lock(&shdata->manager_lock);
    pthread_mutex_unlock(&shdata->stop_lock);

    // Waits for the condition to be fullfiled for abstime seconds, after either of
    // both conditions is fullfiled the lock is aquired when possible to continue.
    pthread_cond_timedwait(&shdata->manager_cond, &shdata->manager_lock, abstime);

    // Aquire the lock when I wake up
    pthread_mutex_lock(&shdata->stop_lock);
    pthread_mutex_unlock(&shdata->manager_lock);
}

pid_t shmem_mngo_get_pid(const size_t mid) {
    return shdata->manager_info[mid].pid;
}

size_t shmem_mngo_get_max_size(void) {
    return max_attached;
}

void shmem_mngo__alltoall_actions(size_t mid, mngo_actions_t *actions) {
    int error = assert_manager_active(mid);
    if (error != DLB_SUCCESS) {
        fatal("Trying to use the MNGO shared memory while uninitialized, at %s", __func__);
    }
    
    shmem_lock(shm_handler);
    {
        shdata->manager_info[mid].actions = *actions;
    }    
    shmem_unlock(shm_handler);

    shmem_mngo_barrier_wait();

    mngo_actions_t result = MNGO_ACTION_NONE;
    // Collect the results
    for (size_t i = 0; i < max_attached; i++) {
        mngo_manager_info_t *info = &shdata->manager_info[i];
        if (info->pid == 0) continue;
        result = result > info->actions ? result : info->actions;
    }

    *actions = result;
}

int shmem_mngo_drom__alltoall_deltas_start(size_t mid, int self_cpu_delta) {
    int error = assert_manager_active(mid);
    if (error != DLB_SUCCESS) {
        fatal("Trying to use the MNGO shared memory while uninitialized, at %s", __func__);
    }

    // Share self intention
    shmem_lock(shm_handler);
    {
        shdata->manager_info[mid].drom_core_change = self_cpu_delta;
    }
    shmem_unlock(shm_handler);

    shmem_mngo_barrier_wait();

    return max_attached;
}

void shmem_mngo_drom__alltoall_deltas_finish(size_t mid, int *cpu_deltas) {
    // Collect the results
    for (size_t i = 0; i < max_attached; i++) {
        if (shdata->manager_info[i].pid == 0) {
            cpu_deltas[i] = 0;
        } else {
            cpu_deltas[i] = shdata->manager_info[i].drom_core_change;
        }
    }

    shmem_mngo_barrier_wait();

    // Clean up axiliar variables in shared memory
    if (mid == 0) {
        shmem_lock(shm_handler);
        {
            for (unsigned int id = 0; id < max_attached; id++) {
              shdata->manager_info[id].drom_core_change = 0;
            }
        }
        shmem_unlock(shm_handler);
    }

    shmem_mngo_barrier_wait();
}

static char * shmem_mngo_print_redistribution_val = NULL;

void shmem_mngo_drom__print_redistribution(int mid, int dcores, const char* region_name) {
    pid_t pid = shmem_mngo_get_pid(mid);

    cpu_set_t mask;
    shmem_procinfo__getprocessmask(pid, &mask, DLB_DROM_FLAGS_NONE);

    shmem_mngo_barrier_wait();
    {
        shdata->manager_info[mid].dcores = dcores;
        shdata->manager_info[mid].ncores = mu_count_cores_intersecting_with_cpuset(&mask);
    }
    shmem_mngo_barrier_wait();

    if (mid == 0) {
        // TODO: Move to shmem_mngo_init
        if (shmem_mngo_print_redistribution_val == NULL) {
            shmem_mngo_print_redistribution_val = malloc(sizeof(char) * 512);
        }

        char *buff = shmem_mngo_print_redistribution_val;

        shmem_lock(shm_handler);
        {
            int i;
            int size = 0;

            bool changed = false;
            int max_ncores = 0;
            int max_rank  = 0;
            for ( i = 0; i < shdata->attached; i++) {
                int curr_ncores = shdata->manager_info[i].ncores;
                max_ncores = curr_ncores > max_ncores ? curr_ncores : max_ncores;

                int curr_dcores = shdata->manager_info[i].dcores;
                changed |= curr_dcores != 0;

                int curr_rank = shdata->manager_info[i].rank;
                max_rank = curr_rank > max_rank ? curr_rank : max_rank;
            }

            if (changed) {
                static const char *format =  " [ %*d: %*d (%+*d) ] ";

                int digits_rank = snprintf(NULL, 0, "%d", max_rank);
                int digits_cores = snprintf(NULL, 0, "%d", max_ncores);
                int col_size   = snprintf(NULL, 0, format,
                        digits_rank, 0,
                        digits_cores, 0,
                        digits_cores, 0);

                int num_cols = 80 / col_size;
                int line_width = num_cols * col_size;

                for (i = 0; i < shdata->attached; i++) {
                    if (i % num_cols == 0) size += snprintf(&buff[size], 512 - size, "\n");

                    size += snprintf(&buff[size], 512 - size, format,
                            digits_rank, shdata->manager_info[i].rank,
                            digits_cores, shdata->manager_info[i].ncores,
                            digits_cores, shdata->manager_info[i].dcores);
                }

                static const char title[] =  "MNGO REDISTRIBUTION REPORT [ rank: #cores (#change) ]";
                static const int title_len = sizeof(title) - 1; // -1 the null terminator

                info("Region: %s\n%*s%*s%s\n", region_name,
                    line_width/2+title_len/2, title, line_width/2-title_len/2, "", // Center the title
                    shmem_mngo_print_redistribution_val);
            }
        }
        shmem_unlock(shm_handler);
    }
}

// This function redistribues the Cores among processes acording to the data in
// <actions>
//
// Invariant:
//  - All the MNGO managers of the same node must call the routine.
//  - actions->self_drom_core_change of all MNGO managers must add up to 0
void shmem_mngo_drom__redistribute(size_t mid, int cpu_delta, cpu_set_t *self_mask) {

    int error = assert_manager_active(mid);
    if (error != DLB_SUCCESS) {
        fatal("Trying to use the MNGO shared memory while uninitialized, at %s", __func__);
    }

    pid_t pid = shmem_mngo_get_pid(mid);
    shmem_procinfo__getprocessmask(pid, self_mask, DLB_DROM_FLAGS_NONE);

    // Handle the case where the number of resources of this process has to be
    // reduced (namely, "giving" processes) by removing Cores from the process
    // mask.
    if (cpu_delta < 0) {
        cpu_set_t mask_freed;
        CPU_ZERO(&mask_freed);

        // Release as many cores as self_crom_core_change, and add them in a local free mask
        int released_cores;
        for(released_cores = 0;
            released_cores < -cpu_delta;
            released_cores++) {

            cpu_set_t mask_release_cpu;

            int release_cpu = mu_get_last_cpu(self_mask);
            // When there are CPUs available
            if ( release_cpu >= 0 ) {
                // Get the set CPUs that are in the same core as "release_cpu" and also in maks_ptr
                mu_and(&mask_release_cpu, self_mask, mu_get_core_mask(release_cpu)->set);
                // Remove those CPUs from mask_ptr
                mu_subtract(self_mask, self_mask, &mask_release_cpu);
                // Add them into mask_freed
                mu_or(&mask_freed, &mask_freed, &mask_release_cpu);
            } else {
                // Something bad happened and we have released all cores.
                fatal("accidentally released all cpus in this process. This is a bug :O");
            }
        }


        // Update the MNGO free mask with the local free mask.
        shmem_lock(shm_handler);
        {
            verbose(VB_MNGO, "SHMEM MNGO [%zu]: New Released %s",mid, mu_to_str(self_mask));
            verbose(VB_MNGO, "SHMEM MNGO [%zu]: Freed %s",mid,  mu_to_str(&mask_freed));
            mu_or(&shdata->available_cpus, &shdata->available_cpus, &mask_freed);
        }
        shmem_unlock(shm_handler);
    }

    // Wait for all the processes to release the CPUs
    shmem_mngo_barrier_wait();
    verbose(VB_MNGO, "SHMEM MNGO [%zu]: Avail %s", mid, mu_to_str(&shdata->available_cpus));

    // Handle the case where the number of resources of this process has to
    // increase (namely, "receiving" processes) by adding CPUs previously freed
    // by the "giving" processes.
    if (cpu_delta > 0) {
        shmem_lock(shm_handler);
        {
            cpu_set_t *mask_free = &shdata->available_cpus;

            int num_aquired_cores;
            for(num_aquired_cores = 0;
                    num_aquired_cores < cpu_delta;
                    num_aquired_cores++) {
                // Find the las CPU available in the mask_free
                int cpu_to_aquire = mu_get_last_cpu(mask_free);
                // When there are CPUs left
                if ( cpu_to_aquire >= 0 ) {
                    // Get the set of CPUs that are in the same core as
                    // "cpu_to_aquire" and also in maks_free.
                    cpu_set_t mask_core_to_aquire;
                    mu_and(&mask_core_to_aquire,
                            mask_free, mu_get_core_mask(cpu_to_aquire)->set);
                    // Remove those CPU from mask_free
                    mu_subtract(mask_free, mask_free, &mask_core_to_aquire);
                    // Add them into the process mask
                    mu_or(self_mask, self_mask, &mask_core_to_aquire);
                } else {
                    // We are trying to aquire more cores than what was
                    // freed. No big deal just break.
                    warning("Trying to aquire more cpus than available");
                    break;
                }
            }
            verbose(VB_MNGO, "SHMEM MNGO [%zu]: New Added %s",mid, mu_to_str(self_mask));
        }
        shmem_unlock(shm_handler);
    }
}

void shmem_mngo__print_info(const char *shmem_key) {

    if (! shmem_exists(shmem_name, shmem_key)) {
        return;
    }

    /* If the shmem is not opened, obtain a temporary fd */
    bool temporary_shmem = shm_handler == NULL;
    if (temporary_shmem) {
        shmem_mngo_ext__init(shmem_key);
    }

    /* Make a full copy of the shared memory */
    shdata_t *shdata_copy = malloc(shmem_mngo_size());
    shmem_lock(shm_handler);
    {
        memcpy(shdata_copy, shdata, shmem_mngo_size());
    }
    shmem_unlock(shm_handler);

    /* Close shmem if needed */
    if (temporary_shmem) {
        shmem_mngo_ext__finalize();
    }

    /* Initialize buffer */
    print_buffer_t buffer;
    printbuffer_init(&buffer);

    /* Set up line buffer */
    enum { MAX_LINE_LEN = 128 };
    char line[MAX_LINE_LEN];

    size_t mid;
    for (mid = 0; mid < max_attached; ++mid) {

        if (shdata->manager_info[mid].pid) {
            /* Append line to buffer */
            snprintf(line, MAX_LINE_LEN,
                     "NOT IMPLEMENTED");
            printbuffer_append(&buffer, line);
        }
    }


    if (buffer.addr[0] != '\0' ) {
        info0("=== MNGO ===\n"
              "  | %12s | %12s | %12s | %12s |\n"
              "%s", "Manager ID", "Finalize", "LeWI", "DROM", buffer.addr);
    }
    printbuffer_destroy(&buffer);
    free(shdata_copy);
    shdata_copy = NULL;
}

/**
 *  Test functions
 */
void test_shmem_mngo__modify_barrier_participants(int participants) {
    _mngo_test_barrier_participants = participants;
}
