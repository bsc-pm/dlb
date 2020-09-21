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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "LB_comm/shmem_procinfo.h"

#include "LB_comm/shmem.h"
#include "LB_numThreads/numThreads.h"
#include "apis/dlb_errors.h"
#include "support/debug.h"
#include "support/types.h"
#include "support/mytime.h"
#include "support/mask_utils.h"
#include "LB_core/DLB_talp.h"
#include "LB_core/spd.h"

#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/resource.h>

enum { NOBODY = 0 };
enum { SYNC_POLL_DELAY = 10000 };           /* 10^4 ns = 10 ms */
enum { SYNC_POLL_TIMEOUT = 1000000000 };    /* 10^9 ns = 1s */

//static const long UPDATE_USAGE_MIN_THRESHOLD    =  100000000L;   // 10^8 ns = 100ms
//static const long UPDATE_LOADAVG_MIN_THRESHOLD  = 1000000000L;   // 10^9 ns = 1s
//static const double LOG2E = 1.44269504088896340736;
typedef struct monitor_info_t {
    char *placeholder1;
    struct timespec  mpi_time;
    struct timespec  compute_time;
    struct timespec placeholder2;
    struct timespec placeholder3;
    struct timespec placeholder4;
} monitor_info_t;
typedef struct {
    struct timespec mpi_time;
    struct timespec compute_time;
    double percent;
    int niter;
} iter_t;
typedef struct {
    iter_t iterations[1000];
    int iter_num;
    double load_balance;
    monitor_info_t aux;
} auto_sizer_t;

typedef struct {
    pid_t pid;
    bool dirty;
    bool preregistered;
    cpu_set_t current_process_mask;
    cpu_set_t future_process_mask;
    cpu_set_t stolen_cpus;
    unsigned int active_cpus;
    // Cpu Usage fields:
    double cpu_usage;
    double cpu_avg_usage;
    auto_sizer_t auto_sizer;
    double mpi_time;
    double comp_time;
#ifdef DLB_LOAD_AVERAGE
    // Load average fields:
    float load[3];              // 1min, 5min, 15mins
    struct timespec last_ltime; // Last time that Load was updated
#endif
} pinfo_t;

typedef struct {
    bool initialized;
    struct timespec initial_time;
    cpu_set_t free_mask;        // Contains the CPUs in the system not owned
    cpu_set_t resize_mask;        // Contains the CPUs in the system not owned
    pinfo_t process_info[0];
} shdata_t;

enum { SHMEM_PROCINFO_VERSION = 3 };

static shmem_handler_t *shm_handler = NULL;
static shdata_t *shdata = NULL;
static int max_cpus;
static int max_processes;
//static struct timespec last_ttime; // Total time
//static struct timespec last_utime; // Useful time (user+system)
static const char *shmem_name = "procinfo";
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static int subprocesses_attached = 0;

static int set_new_mask(pinfo_t *process, const cpu_set_t *mask, bool sync);
static void close_shmem(bool shmem_empty);

static pinfo_t* get_process(pid_t pid) {
    if (shdata) {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid == pid) {
                return &shdata->process_info[p];
            }
        }
    }
    return NULL;
}

/*********************************************************************************/
/*  Init / Register                                                              */
/*********************************************************************************/

static void open_shmem(const char *shmem_key) {
    pthread_mutex_lock(&mutex);
    {
        if (shm_handler == NULL) {
            // We assume no more processes than CPUs
            max_cpus = mu_get_system_size();
            max_processes = max_cpus;

            shm_handler = shmem_init((void**)&shdata,
                    sizeof(shdata_t) + sizeof(pinfo_t)*max_processes,
                    shmem_name, shmem_key, SHMEM_PROCINFO_VERSION);
            subprocesses_attached = 1;
        } else {
            ++subprocesses_attached;
        }
    }
    pthread_mutex_unlock(&mutex);
}

// Register a new set of CPUs. Remove them from the free_mask and assign them to new_owner if ok
static int register_mask(pinfo_t *new_owner, const cpu_set_t *mask) {
    // Return if empty mask
    if (CPU_COUNT(mask) == 0) return DLB_SUCCESS;

    verbose(VB_DROM, "Process %d registering mask %s", new_owner->pid, mu_to_str(mask));
    int error = DLB_SUCCESS;
    if (mu_is_subset(mask, &shdata->free_mask)) {
        mu_substract(&shdata->free_mask, &shdata->free_mask, mask);
        CPU_OR(&new_owner->future_process_mask, &new_owner->future_process_mask, mask);
        mu_substract(&new_owner->stolen_cpus, &new_owner->stolen_cpus, mask);
        new_owner->dirty = true;
    } else {
        cpu_set_t wrong_cpus;
        mu_substract(&wrong_cpus, mask, &shdata->free_mask);
        verbose(VB_SHMEM, "Error registering CPUs: %s, already belong to other processes",
                mu_to_str(&wrong_cpus));
        error = DLB_ERR_PERM;
    }
    return error;
}

int shmem_procinfo__init(pid_t pid, const cpu_set_t *process_mask, cpu_set_t *new_process_mask,
        const char *shmem_key) {
    int error = DLB_SUCCESS;

    // Shared memory creation
    open_shmem(shmem_key);

    pinfo_t *process = NULL;
    shmem_lock(shm_handler);
    {
        // Initialize some values if this is the 1st process attached to the shmem
        if (!shdata->initialized) {
            get_time(&shdata->initial_time);
            mu_get_system_mask(&shdata->free_mask);
            shdata->initialized = true;
        }

        // Find whether the process is preregistered or get a free spot otherwise
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid == pid) {
                process = &shdata->process_info[p];
                if (process->preregistered) {
                    // If the process is preregistered, we must only return the
                    // new_process_mask to cpuinfo to avoid conflicts,
                    // we cannot resolve the dirty flag yet
                    memcpy(new_process_mask,
                            process->dirty ? &process->future_process_mask
                                           : &process->current_process_mask,
                            sizeof(cpu_set_t));
                    process->preregistered = false;
                    error = DLB_NOTED;
                } else {
                    // Otherwise, this pid is already correctly registered
                    error = DLB_ERR_INIT;
                }
                break;
            } else if (!process && shdata->process_info[p].pid == NOBODY) {
                // We obtain the first free spot, but we cannot break
                process = &shdata->process_info[p];
            }
        }

        // Register if needed
        if (process && error == DLB_SUCCESS) {
            error = register_mask(process, process_mask);
            if (error == DLB_SUCCESS) {
                process->pid = pid;
                process->dirty = false;
                process->preregistered = false;
                memcpy(&process->current_process_mask, process_mask, sizeof(cpu_set_t));
                memcpy(&process->future_process_mask, process_mask, sizeof(cpu_set_t));
                process->cpu_usage = 0.0;
                process->cpu_avg_usage = 0.0;

#ifdef DLB_LOAD_AVERAGE
                process->load[0] = 0.0f;
                process->load[1] = 0.0f;
                process->load[2] = 0.0f;
#endif
            }
        }
    }
    shmem_unlock(shm_handler);

    if (process == NULL) {
        verbose(VB_SHMEM,
            "Not enough space in the shared memory to register process %d", pid);
        error = DLB_ERR_NOMEM;
    }

    if (error < DLB_SUCCESS) {
        // The shared memory contents are untouched, but the counter needs to
        // be decreased, and the shared memory deleted if needed
        bool shmem_empty = true;
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid != NOBODY) {
                shmem_empty = false;
                break;
            }
        }
        close_shmem(shmem_empty);
    }

    return error;
}

int shmem_procinfo_ext__init(const char *shmem_key) {
    // Shared memory creation
    open_shmem(shmem_key);

    shmem_lock(shm_handler);
    {
        // Initialize some values if this is the 1st process attached to the shmem
        if (!shdata->initialized) {
            get_time(&shdata->initial_time);
            mu_get_system_mask(&shdata->free_mask);
            shdata->initialized = true;
        }
    }
    shmem_unlock(shm_handler);

    return DLB_SUCCESS;
}

int shmem_procinfo_ext__preinit(pid_t pid, const cpu_set_t *mask, dlb_drom_flags_t flags) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    bool steal = flags & DLB_STEAL_CPUS;
    bool sync = flags & DLB_SYNC_QUERY;
    int error = DLB_SUCCESS;
    pinfo_t *process = NULL;
    shmem_lock(shm_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid == pid) {
                // PID already registered
                error = DLB_ERR_INIT;
                break;
            } else if (shdata->process_info[p].pid == NOBODY) {
                process = &shdata->process_info[p];
                process->pid = pid;
                process->dirty = false;
                process->preregistered = true;
                CPU_ZERO(&process->current_process_mask);
                CPU_ZERO(&process->future_process_mask);

                // Register process mask into the system
                if (!steal) {
                    // If no stealing, register as usual
                    error = register_mask(process, mask);
                } else {
                    // Otherwise, steal CPUs if necessary
                    error = set_new_mask(process, mask, sync);
                }
                if (error) {
                    // Release shared memory spot
                    process->pid = NOBODY;
                    break;
                }

                // Set process initial values
                memcpy(&process->current_process_mask, mask, sizeof(cpu_set_t));
                memcpy(&process->future_process_mask, mask, sizeof(cpu_set_t));
                process->dirty = false;

#ifdef DLB_LOAD_AVERAGE
                process->load[0] = 0.0f;
                process->load[1] = 0.0f;
                process->load[2] = 0.0f;
#endif
                break;
            }
        }
    }
    shmem_unlock(shm_handler);

    if (error == DLB_ERR_INIT) {
        verbose(VB_SHMEM, "Process %d already registered", pid);
    } else if (error == DLB_ERR_PERM) {
        verbose(VB_SHMEM,
                "Error trying to register CPU mask: %s", mu_to_str(mask));
    } else if (process == NULL) {
        verbose(VB_SHMEM,
            "Not enough space in the shared memory to register process %d", pid);
        error = DLB_ERR_NOMEM;
    }

    return error;
}


/*********************************************************************************/
/*  Finalize / Unregister                                                        */
/*********************************************************************************/

// Unregister CPUs. Add them to the free_mask or give them back to their owner
static int unregister_mask(pinfo_t *owner, const cpu_set_t *mask, bool return_stolen) {
    // Return if empty mask
    if (CPU_COUNT(mask) == 0) return DLB_SUCCESS;

    verbose(VB_DROM, "Process %d unregistering mask %s", owner->pid, mu_to_str(mask));
    if (return_stolen) {
        // Look if each CPU belongs to some other process
        int c, p;
        for (c = 0; c < max_cpus; c++) {
            if (CPU_ISSET(c, mask)) {
                for (p = 0; p < max_processes; p++) {
                    pinfo_t *process = &shdata->process_info[p];
                    if (process->pid != NOBODY && CPU_ISSET(c, &process->stolen_cpus)) {
                        // give it back to the process
                        CPU_SET(c, &process->future_process_mask);
                        CPU_CLR(c, &process->stolen_cpus);
                        process->dirty = true;
                        verbose(VB_DROM, "Giving back CPU %d to process %d", c, process->pid);
                        break;
                    }
                }
                // if we didn't find the owner, add it to the free_mask
                if (p == max_processes) {
                    CPU_SET(c, &shdata->free_mask);
                }
                // remove CPU from owner
                CPU_CLR(c, &owner->future_process_mask);
                owner->dirty = true;
            }
        }
    } else {
        // Add mask to free_mask and remove them from owner
        CPU_OR(&shdata->free_mask, &shdata->free_mask, mask);
        mu_substract(&owner->future_process_mask, &owner->future_process_mask, mask);
        owner->dirty = true;
    }
    return DLB_SUCCESS;
}

static void close_shmem(bool shmem_empty) {
    pthread_mutex_lock(&mutex);
    {
        if (--subprocesses_attached == 0) {
            shmem_finalize(shm_handler, shmem_empty ? SHMEM_DELETE : SHMEM_NODELETE);
            shm_handler = NULL;
            shdata = NULL;
        }
    }
    pthread_mutex_unlock(&mutex);
}

int shmem_procinfo__finalize(pid_t pid, bool return_stolen, const char *shmem_key) {
    int error = DLB_SUCCESS;
    bool shmem_empty = true;
    bool shmem_reopened = false;

    if (shm_handler == NULL) {
        /* procinfo_finalize may be called to finalize existing process
         * even if the file descriptor is not opened. (DLB_PreInit + forc-exec case) */
        if (shmem_exists(shmem_name, shmem_key)) {
            open_shmem(shmem_key);
            shmem_reopened = true;
        } else {
            return DLB_ERR_NOSHMEM;
        }
    }

    // Check that pid exists
    pinfo_t *process = get_process(pid);
    if (unlikely(process == NULL)) {
        error = DLB_ERR_NOPROC;
    }

    shmem_lock(shm_handler);
    {
        if (process) {
            // Unregister our process mask, or future mask if we are dirty
            if (process->dirty) {
                unregister_mask(process, &process->future_process_mask, return_stolen);
            } else {
                unregister_mask(process, &process->current_process_mask, return_stolen);
            }

            // Clear process fields
            process->pid = NOBODY;
            process->dirty = false;
            process->preregistered = false;
            CPU_ZERO(&process->current_process_mask);
            CPU_ZERO(&process->future_process_mask);
            CPU_ZERO(&process->stolen_cpus);
            process->active_cpus = 0;
            process->cpu_usage = 0.0;
            process->cpu_avg_usage = 0.0;
#ifdef DLB_LOAD_AVERAGE
            process->load[3] = {0.0f, 0.0f, 0.0f};
            process->last_ltime = {0};
#endif
        }

        // Check if shmem is empty
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid != NOBODY) {
                shmem_empty = false;
                break;
            }
        }
    }
    shmem_unlock(shm_handler);

    // Close shared memory only if pid was succesfully removed or if shmem was reopened
    if (process || shmem_reopened) {
        close_shmem(shmem_empty);
    }

    return error;
}

int shmem_procinfo_ext__finalize(void) {
    // Protect double finalization
    if (shm_handler == NULL) {
        return DLB_ERR_NOSHMEM;
    }

    // Check if shmem is empty
    bool shmem_empty = true;
    shmem_lock(shm_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid != NOBODY) {
                shmem_empty = false;
                break;
            }
        }
    }
    shmem_unlock(shm_handler);

    // Shared memory destruction
    close_shmem(shmem_empty);

    return DLB_SUCCESS;
}

int shmem_procinfo_ext__postfinalize(pid_t pid, bool return_stolen) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    int error = DLB_SUCCESS;
    shmem_lock(shm_handler);
    {
        pinfo_t *process = get_process(pid);
        if (process == NULL) {
            verbose(VB_DROM, "Cannot finalize process %d", pid);
            error = DLB_ERR_NOPROC;
        } else {
            // Unregister process mask, or future mask if dirty
            if (process->dirty) {
                unregister_mask(process, &process->future_process_mask, return_stolen);
            } else {
                unregister_mask(process, &process->current_process_mask, return_stolen);
            }

            // Clear process fields
            process->pid = NOBODY;
            process->dirty = false;
            process->preregistered = false;
            CPU_ZERO(&process->current_process_mask);
            CPU_ZERO(&process->future_process_mask);
            CPU_ZERO(&process->stolen_cpus);
            process->active_cpus = 0;
            process->cpu_usage = 0.0;
            process->cpu_avg_usage = 0.0;
#ifdef DLB_LOAD_AVERAGE
            process->load[3] = {0.0f, 0.0f, 0.0f};
            process->last_ltime = {0};
#endif
        }
    }
    shmem_unlock(shm_handler);
    return error;
}

int shmem_procinfo_ext__recover_stolen_cpus(int pid) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    int error = DLB_SUCCESS;
    shmem_lock(shm_handler);
    {
        pinfo_t *process = get_process(pid);
        if (process == NULL) {
            verbose(VB_DROM, "Cannot find process %d", pid);
            error = DLB_ERR_NOPROC;
        } else {
            // Recover all stolen CPUs only if the CPU is set in the free_mask
            cpu_set_t recovered_cpus;
            CPU_AND(&recovered_cpus, &process->stolen_cpus, &shdata->free_mask);
            error = register_mask(process, &recovered_cpus);
            if (error == DLB_SUCCESS) {
                mu_substract(&process->stolen_cpus, &process->stolen_cpus, &recovered_cpus);
            }
        }
    }
    shmem_unlock(shm_handler);
    return error;
}


/*********************************************************************************/
/* Get / Set Process mask                                                        */
/*********************************************************************************/

int shmem_procinfo__getprocessmask(pid_t pid, cpu_set_t *mask, dlb_drom_flags_t flags) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    int error = DLB_SUCCESS;
    bool done = false;
    pinfo_t *process;
    shmem_lock(shm_handler);
    {
        // Find process
        process = get_process(pid);
        if (process == NULL) {
            verbose(VB_DROM, "Getting mask: cannot find process with pid %d", pid);
            error = DLB_ERR_NOPROC;
        }

        if (!error) {
            if (!process->dirty) {
                // Get current mask if not dirty
                memcpy(mask, &process->current_process_mask, sizeof(cpu_set_t));
                done = true;
            } else if (!(flags & DLB_SYNC_QUERY)) {
                // Get future mask if query is non-blocking
                memcpy(mask, &process->future_process_mask, sizeof(cpu_set_t));
                done = true;
            }
        }
    }
    shmem_unlock(shm_handler);

    if (!error && !done) {
        // process is valid, but it's dirty so we need to poll
        int64_t elapsed;
        struct timespec start, now;
        get_time_coarse(&start);
        while(true) {

            // Delay
            usleep(SYNC_POLL_DELAY);

            // Polling
            shmem_lock(shm_handler);
            {
                if (!process->dirty) {
                    memcpy(mask, &process->current_process_mask, sizeof(cpu_set_t));
                    done = true;
                }
            }
            shmem_unlock(shm_handler);

            // Break if done
            if (done) break;

            // Break if timeout
            get_time_coarse(&now);
            elapsed = timespec_diff(&start, &now);
            if (elapsed > SYNC_POLL_TIMEOUT) {
                error = DLB_ERR_TIMEOUT;
                break;
            }
        }
    }

    return error;
}

int shmem_procinfo__setprocessmask(pid_t pid, const cpu_set_t *mask, dlb_drom_flags_t flags) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    bool sync = flags & DLB_SYNC_QUERY;
    int error = DLB_SUCCESS;
    pinfo_t *process;
    shmem_lock(shm_handler);
    {
        // Find process
        process = get_process(pid);
        if (process == NULL) {
            error = DLB_ERR_NOPROC;
        }

        // Process already dirty
        if (!error && process->dirty) {
            error = DLB_ERR_PDIRTY;
        }

        // Set new mask if everything ok
        error = error ? error : set_new_mask(process, mask, sync);
    }
    shmem_unlock(shm_handler);

    // Polling until dirty is cleared
    if (!error && sync) {
        bool done = false;
        struct timespec start, now;
        get_time_coarse(&start);
        do {

            // Delay
            usleep(SYNC_POLL_DELAY);

            // Poll
            shmem_lock(shm_handler);
            {
                if (process->pid != pid) {
                    // process no longer valid
                    error = DLB_ERR_NOPROC;
                    done = true;
                }

                if (!process->dirty) {
                    done = true;
                }
            }
            shmem_unlock(shm_handler);

            // Check timeout
            if (!done) {
                get_time_coarse(&now);
                if (timespec_diff(&start, &now) > SYNC_POLL_TIMEOUT) {
                    error = DLB_ERR_TIMEOUT;
                }
            }
        } while (!done && error == DLB_SUCCESS);
    }

    if (error == DLB_ERR_NOPROC) {
        verbose(VB_DROM, "Setting mask: cannot find process with pid %d", pid);
    } else if (error == DLB_ERR_PDIRTY) {
        verbose(VB_DROM, "Setting mask: process %d is already dirty", pid);
    } else if (error == DLB_ERR_PERM) {
        verbose(VB_DROM, "Setting mask: cannot steal mask %s", mu_to_str(mask));
    }

    return error;
}


/*********************************************************************************/
/* Generic Getters                                                               */
/*********************************************************************************/

int shmem_procinfo__polldrom(pid_t pid, int *new_cpus, cpu_set_t *new_mask) {
    int error;
    if (shm_handler == NULL) {
        error = DLB_ERR_NOSHMEM;
    } else {
        pinfo_t *process = get_process(pid);
        if (!process) {
            error = DLB_ERR_NOPROC;
        } else if (!process->dirty) {
            error = DLB_NOUPDT;
        } else {
            shmem_lock(shm_handler);
            {
                // Update output parameters
                memcpy(new_mask, &process->future_process_mask, sizeof(cpu_set_t));
                if (new_cpus != NULL) *new_cpus = CPU_COUNT(&process->future_process_mask);

                // Upate local info
                memcpy(&process->current_process_mask, &process->future_process_mask,
                        sizeof(cpu_set_t));
                process->dirty = false;
            }
            shmem_unlock(shm_handler);
            error = DLB_SUCCESS;
        }
    }
    return error;
}

int shmem_procinfo__getpidlist(pid_t *pidlist, int *nelems, int max_len) {
    *nelems = 0;
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;
    shmem_lock(shm_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            pid_t pid = shdata->process_info[p].pid;
            if (pid != NOBODY) {
                pidlist[(*nelems)++] = pid;
            }
            if (*nelems == max_len) {
                break;
            }
        }
    }
    shmem_unlock(shm_handler);
    return DLB_SUCCESS;
}


/*********************************************************************************/
/* Statistics                                                                    */
/*********************************************************************************/

double shmem_procinfo__getcpuusage(pid_t pid) {
    if (shm_handler == NULL) return -1.0;

    double cpu_usage = -1.0;
    shmem_lock(shm_handler);
    {
        pinfo_t *process = get_process(pid);
        if (process) {
            cpu_usage = process->cpu_usage;
        }
    }
    shmem_unlock(shm_handler);

    return cpu_usage;
}

double shmem_procinfo__getcpuavgusage(pid_t pid) {
    if (shm_handler == NULL) return -1.0;

    double cpu_avg_usage = -1.0;
    shmem_lock(shm_handler);
    {
        pinfo_t *process = get_process(pid);
        if (process) {
            cpu_avg_usage = process->cpu_avg_usage;
        }
    }
    shmem_unlock(shm_handler);

    return cpu_avg_usage;
}

void shmem_procinfo__getcpuusage_list(double *usagelist, int *nelems, int max_len) {
    *nelems = 0;
    if (shm_handler == NULL) return;
    shmem_lock(shm_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid != NOBODY) {
                usagelist[(*nelems)++] = shdata->process_info[p].cpu_usage;
            }
            if (*nelems == max_len) {
                break;
            }
        }
    }
    shmem_unlock(shm_handler);
}

void shmem_procinfo__getcpuavgusage_list(double *avgusagelist, int *nelems, int max_len) {
    *nelems = 0;
    if (shm_handler == NULL) return;
    shmem_lock(shm_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid != NOBODY) {
                avgusagelist[(*nelems)++] = shdata->process_info[p].cpu_avg_usage;
            }
            if (*nelems == max_len) {
                break;
            }
        }
    }
    shmem_unlock(shm_handler);
}

double shmem_procinfo__getnodeusage(void) {
    if (shm_handler == NULL) return -1.0;

    double cpu_usage = 0.0;
    shmem_lock(shm_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid != NOBODY) {
                cpu_usage += shdata->process_info[p].cpu_usage;
            }
        }
    }
    shmem_unlock(shm_handler);

    return cpu_usage;
}

double shmem_procinfo__getnodeavgusage(void) {
    if (shm_handler == NULL) return -1.0;

    double cpu_avg_usage = 0.0;
    shmem_lock(shm_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid != NOBODY) {
                cpu_avg_usage += shdata->process_info[p].cpu_avg_usage;
            }
        }
    }
    shmem_unlock(shm_handler);

    return cpu_avg_usage;
}

int shmem_procinfo__getactivecpus(pid_t pid) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;

    int active_cpus = -1;
    shmem_lock(shm_handler);
    {
        pinfo_t *process = get_process(pid);
        if (process) {
            active_cpus = process->active_cpus;
        }
    }
    shmem_unlock(shm_handler);
    return active_cpus;
}

void shmem_procinfo__getactivecpus_list(pid_t *cpuslist, int *nelems, int max_len) {
    *nelems = 0;
    if (shm_handler == NULL) return;
    shmem_lock(shm_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid != NOBODY) {
                cpuslist[(*nelems)++] = shdata->process_info[p].active_cpus;
            }
            if (*nelems == max_len) {
                break;
            }
        }
    }
    shmem_unlock(shm_handler);
}

int shmem_procinfo__getloadavg(pid_t pid, double *load) {
    if (shm_handler == NULL) return DLB_ERR_NOSHMEM;
    int error = DLB_ERR_UNKNOWN;
#ifdef DLB_LOAD_AVERAGE
    shmem_lock(shm_handler);
    {
        pinfo_t *process = get_process(pid);
        if (process) {
            load[0] = process->load[0];
            load[1] = process->load[1];
            load[2] = process->load[2];
            error = 0;
        }
    }
    shmem_unlock(shm_handler);
#endif
    return error;
}


int  shmem_procinfo__getmpitime(pid_t pid, double *mpi_time){

        pinfo_t *process = get_process(pid);
        if (process) {
            *mpi_time = process->mpi_time;
            return DLB_SUCCESS;
        }
        else
            return -1;
}

int  shmem_procinfo__getcomptime(pid_t pid, double *comp_time){

        pinfo_t *process = get_process(pid);
        if (process) {
            *comp_time = process->comp_time;
            return DLB_SUCCESS;
        }
        else
            return -1;
}



int shmem_procinfo__setcpuavgusage(pid_t pid, double new_avg_usage) {

    if (shm_handler == NULL) return -1.0;

    shmem_lock(shm_handler);
    {
        pinfo_t *process = get_process(pid);
        if (process) {
            process->cpu_avg_usage = new_avg_usage;
        }
    }
    shmem_unlock(shm_handler);

    return DLB_SUCCESS;
}

int shmem_procinfo__setcpuusage(pid_t pid,int index, double new_avg_usage) {
    if (shm_handler == NULL) return -1.0;

    shmem_lock(shm_handler);
    {
        pinfo_t *process = get_process(pid);
        if (process) {
            process->cpu_avg_usage = new_avg_usage;
        }
    }
    shmem_unlock(shm_handler);

    return DLB_SUCCESS;
}

int shmem_procinfo__setcomptime(pid_t pid, double new_comp_time) {

    if (shm_handler == NULL) return -1.0;

    shmem_lock(shm_handler);
    {
        pinfo_t *process = get_process(pid);
        if (process) {
            process->comp_time = new_comp_time;
        }
    }
    shmem_unlock(shm_handler);

    return DLB_SUCCESS;
}

int shmem_procinfo__setmpitime(pid_t pid, double new_mpi_time) {

    if (shm_handler == NULL) return -1.0;

    shmem_lock(shm_handler);
    {
        pinfo_t *process = get_process(pid);
        if (process) {
            process->mpi_time = new_mpi_time;
        }
    }
    shmem_unlock(shm_handler);

    return DLB_SUCCESS;
}


/*********************************************************************************/
/* Misc                                                                          */
/*********************************************************************************/

void shmem_procinfo__print_info(const char *shmem_key) {

    /* If the shmem is not opened, obtain a temporary fd */
    bool temporary_shmem = shm_handler == NULL;
    if (temporary_shmem) {
        shmem_procinfo_ext__init(shmem_key);
    }

    /* Make a full copy of the shared memory */
    shdata_t *shdata_copy = malloc(sizeof(shdata_t) + sizeof(pinfo_t)*max_processes);
    shmem_lock(shm_handler);
    {
        memcpy(shdata_copy, shdata, sizeof(shdata_t) + sizeof(pinfo_t)*max_processes);
    }
    shmem_unlock(shm_handler);

    /* Close shmem if needed */
    if (temporary_shmem) {
        shmem_procinfo_ext__finalize();
    }

    /* Find the max number of characters per column */
    pid_t max_pid = 111;    /* 3 digits for 'PID' */
    int max_current = 4;    /* 'Mask' */
    int max_future = 6;     /* 'Future' */
    int max_stolen = 6;     /* 'Stolen' */
    int p;
    for (p = 0; p < max_processes; ++p) {
        pinfo_t *process = &shdata_copy->process_info[p];
        if (process->pid != NOBODY) {
            size_t len;
            /* pid */
            max_pid = process->pid > max_pid ? process->pid : max_pid;
            /* current_mask */
            len = strlen(mu_to_str(&process->current_process_mask));
            max_current = len > max_current ? len : max_current;
            /* future_mask */
            len = strlen(mu_to_str(&process->future_process_mask));
            max_future = len > max_future ? len : max_future;
            /* stolen_mask */
            len = strlen(mu_to_str(&process->stolen_cpus));
            max_stolen = len > max_stolen ? len : max_stolen;
        }
    }
    int max_pid_digits = snprintf(NULL, 0, "%d", max_pid);

    /* Initialize buffer */
    print_buffer_t buffer;
    printbuffer_init(&buffer);

    /* Set up line buffer */
    enum { MAX_LINE_LEN = 512 };
    char line[MAX_LINE_LEN];

    for (p = 0; p < max_processes; p++) {
        pinfo_t *process = &shdata_copy->process_info[p];
        if (process->pid != NOBODY) {
            const char *mask_str;

            /* Copy current mask */
            mask_str = mu_to_str(&process->current_process_mask);
            char *current = malloc((strlen(mask_str)+1)*sizeof(char));
            strcpy(current, mask_str);

            /* Copy future mask */
            mask_str = mu_to_str(&process->future_process_mask);
            char *future = malloc((strlen(mask_str)+1)*sizeof(char));
            strcpy(future, mask_str);

            /* Copy stolen mask */
            mask_str = mu_to_str(&process->stolen_cpus);
            char *stolen = malloc((strlen(mask_str)+1)*sizeof(char));
            strcpy(stolen, mask_str);

            /* Append line to buffer */
            snprintf(line, MAX_LINE_LEN,
                    "  | %*d | %*s | %*s | %*s | %6d |",
                    max_pid_digits, process->pid,
                    max_current, current,
                    max_future, future,
                    max_stolen, stolen,
                    process->dirty);
            printbuffer_append(&buffer, line);

            free(current);
            free(future);
            free(stolen);
        }
    }

    if (buffer.addr[0] != '\0' ) {
        /* Construct header */
        snprintf(line, MAX_LINE_LEN,
                "  | %*s | %*s | %*s | %*s | Dirty? |",
                max_pid_digits, "PID",
                max_current, "Mask",
                max_future, "Future",
                max_stolen, "Stolen");

        /* Print header + buffer */
        info0("=== Processes Masks ===\n"
              "%s\n"
              "%s", line, buffer.addr);
    }
    printbuffer_destroy(&buffer);
    free(shdata_copy);
}

bool shmem_procinfo__exists(void) {
    return shm_handler != NULL;
}

int shmem_procinfo__version(void) {
    return SHMEM_PROCINFO_VERSION;
}

size_t shmem_procinfo__size(void) {
    return sizeof(shdata_t) + sizeof(pinfo_t)*mu_get_system_size();
}


/*** Helper functions, the shm lock must have been acquired beforehand ***/


// FIXME Update statistics temporarily disabled
#if 0
static void update_process_loads(void) {
    // Get the active CPUs
    cpu_set_t mask;
    memcpy(&mask, &global_spd.active_mask, sizeof(cpu_set_t));
    shdata->process_info[my_process].active_cpus = CPU_COUNT(&mask);

    // Compute elapsed total time
    struct timespec current_ttime;
    int64_t elapsed_ttime_since_last;
    int64_t elapsed_ttime_since_init;;
    get_time_coarse(&current_ttime);
    elapsed_ttime_since_last = timespec_diff(&last_ttime, &current_ttime);
    elapsed_ttime_since_init = timespec_diff(&shdata->initial_time, &current_ttime );

    // Compute elapsed useful time (user+system)
    struct rusage usage;
    struct timespec current_utime;
    int64_t elapsed_utime_since_last;
    int64_t elapsed_utime_since_init;
    getrusage(RUSAGE_SELF, &usage);
    add_tv_to_ts(&usage.ru_utime, &usage.ru_stime, &current_utime);
    elapsed_utime_since_last = timespec_diff(&last_utime, &current_utime);
    elapsed_utime_since_init = to_nsecs(&current_utime);

    // Update times for next update
    last_ttime = current_ttime;
    last_utime = current_utime;

    // Compute usage
    shdata->process_info[my_process].cpu_usage = 100 *
        (double)elapsed_utime_since_last / (double)elapsed_ttime_since_last;

    // Compute avg usage
    shdata->process_info[my_process].cpu_avg_usage = 100 *
        (double)elapsed_utime_since_init / (double)elapsed_ttime_since_init;

#ifdef DLB_LOAD_AVERAGE
    // Do not update the Load Average if the elapsed is less that a threshold
    if (elapsed_ttime > UPDATE_LOADAVG_MIN_THRESHOLD) {
        shdata->process_info[my_process].last_ltime = current_ttime;
        // WIP
    }
#endif
}
#endif


// Steal every CPU in mask from other processes
static int steal_mask(pinfo_t* new_owner, const cpu_set_t *mask, bool sync, bool dry_run) {
    // Return if empty mask
    if (CPU_COUNT(mask) == 0) return DLB_SUCCESS;

    int error = DLB_SUCCESS;
    cpu_set_t cpus_left_to_steal;
    memcpy(&cpus_left_to_steal, mask, sizeof(cpu_set_t));

    // Iterate per process, steal in batch
    int p;
    for (p = 0; p < max_processes; ++p) {
        pinfo_t *victim = &shdata->process_info[p];
        if (victim != new_owner && victim->pid != NOBODY) {
            cpu_set_t target_cpus;
            CPU_AND(&target_cpus, &victim->current_process_mask, mask);
            if (CPU_COUNT(&target_cpus) > 0) {
                // victim contains target CPUs
                if (!victim->dirty &&
                        CPU_COUNT(&victim->current_process_mask) - CPU_COUNT(&target_cpus) > 0) {
                    // Steal target_cpus from victim
                    if (!dry_run) {
                        victim->dirty = true;
                        mu_substract(&victim->future_process_mask,
                                &victim->current_process_mask, &target_cpus);
                        CPU_OR(&victim->stolen_cpus, &victim->stolen_cpus, &target_cpus);
                        verbose(VB_DROM, "CPUs %s have been removed from process %d",
                                mu_to_str(mask), victim->pid);
                    }
                    mu_substract(&cpus_left_to_steal, &cpus_left_to_steal, &target_cpus);
                    if (CPU_COUNT(&cpus_left_to_steal) == 0)
                        break;
                } else {
                    error = DLB_ERR_PERM;
                    break;
                }
            }
        }
    }

    if (__builtin_expect(
                !error && CPU_COUNT(&cpus_left_to_steal) > 0,
                0)) {
        warning("Could not find candidate for stealing mask %s.  Please report to "
                PACKAGE_BUGREPORT, mu_to_str(mask));
        error = DLB_ERR_PERM;
    }

    if (!error && sync && !dry_run) {
        // Relase lock and poll until victims update their masks or timeout
        shmem_unlock(shm_handler);

        bool done = false;
        struct timespec start, now;
        get_time_coarse(&start);
        do {
            // Delay
            usleep(SYNC_POLL_DELAY);

            // Poll
            shmem_lock(shm_handler);
            {
                cpu_set_t all_current_masks;
                CPU_ZERO(&all_current_masks);
                for (p = 0; p < max_processes; ++p) {
                    pinfo_t *victim = &shdata->process_info[p];
                    if (victim != new_owner && victim->pid != NOBODY) {
                        // Accumulate updated masks of other processes
                        CPU_OR(&all_current_masks, &all_current_masks,
                                &victim->current_process_mask);
                    }
                }

                // Polling is complete when no current_mask of any process
                // contains any CPU from the mask we are stealing
                cpu_set_t common_cpus;
                CPU_AND(&common_cpus, &all_current_masks, mask);
                done = CPU_COUNT(&common_cpus) == 0;
            }
            shmem_unlock(shm_handler);

            // Check timeout
            if (!done) {
                get_time_coarse(&now);
                if (timespec_diff(&start, &now) > SYNC_POLL_TIMEOUT) {
                    error = DLB_ERR_TIMEOUT;
                }
            }
        } while (!done && error == DLB_SUCCESS);

        shmem_lock(shm_handler);
    }

    if (!error && !dry_run) {
        /* Assign stolen CPUs to the new owner */
        CPU_OR(&new_owner->future_process_mask, &new_owner->future_process_mask, mask);
        mu_substract(&new_owner->stolen_cpus, &new_owner->stolen_cpus, mask);
        new_owner->dirty = true;
    }

    if (error && !dry_run) {
        // Some CPUs could not be stolen, roll everything back
        for (p = 0; p < max_processes; ++p) {
            pinfo_t *victim = &shdata->process_info[p];
            if (victim != new_owner && victim->pid != NOBODY) {
                cpu_set_t cpus_to_return;
                CPU_AND(&cpus_to_return, &victim->stolen_cpus, mask);
                if (CPU_COUNT(&cpus_to_return) > 0) {
                    // warning: we may return some CPU to a wrong process
                    // if more than one contains that CPU as stolen
                    CPU_OR(&victim->future_process_mask, &victim->future_process_mask,
                            &cpus_to_return);
                    mu_substract(&victim->stolen_cpus, &victim->stolen_cpus, &cpus_to_return);
                    victim->dirty = !CPU_EQUAL(
                            &victim->current_process_mask, &victim->future_process_mask);
                }
            }
        }
    }

    return error;
}


/* Set new mask for process, stealing if necessary
 *  - If the CPU is SET and unused -> register
 *  - If the CPU is SET, used and not owned -> steal
 *  - If the CPU is UNSET and owned by the process -> unregister
 */
static int set_new_mask(pinfo_t *process, const cpu_set_t *mask, bool sync) {
    cpu_set_t cpus_to_acquire;
    cpu_set_t cpus_to_steal;
    cpu_set_t cpus_to_free;
    CPU_ZERO(&cpus_to_acquire);
    CPU_ZERO(&cpus_to_steal);
    CPU_ZERO(&cpus_to_free);

    int c;
    for (c = 0; c < max_cpus; c++) {
        if (CPU_ISSET(c, mask)) {
            if (CPU_ISSET(c, &shdata->free_mask)) {
                // CPU is not being used
                CPU_SET(c, &cpus_to_acquire);
            } else {
                if (!CPU_ISSET(c, &process->future_process_mask)) {
                    // CPU is being used by other process
                    CPU_SET(c, &cpus_to_steal);
                }
            }
        } else {
            if (CPU_ISSET(c, &process->future_process_mask)) {
                // CPU no longer used by this process
                CPU_SET(c, &cpus_to_free);
            }
        }
    }

    /* Run first a dry run to check if CPUs can be stolen */
    int error = steal_mask(process, &cpus_to_steal, sync, /* dry_run */ true);
    error = error ? error : steal_mask(process, &cpus_to_steal, sync, /* dry_run */ false);
    error = error ? error : register_mask(process, &cpus_to_acquire);
    error = error ? error : unregister_mask(process, &cpus_to_free, /* return_stolen */ false);

    return error;
}

int auto_resize_start(){
    if ( !thread_spd->talp_info ) return 0 ;
    pinfo_t *process = get_process(thread_spd->id);
    auto_sizer_t * au = &process->auto_sizer;

    if( au->iter_num == 0)return DLB_SUCCESS;
    shmem_lock(shm_handler);
    {
        // FIXME: fix type
        /* monitoring_region_start(&process->auto_sizer.aux); */
    }
    shmem_unlock(shm_handler);

    int last_iter = au->iter_num - 1;
    int i;
    double temp = process->auto_sizer.iterations[last_iter].percent;

    if( temp < 0.4){
        cpu_set_t mask;
        mask = process->current_process_mask;
        if (CPU_COUNT(&mask) == 1) return DLB_SUCCESS;
        for(i = max_processes - 1; i > 0; i--){
            if(CPU_ISSET(i,&mask)){
                CPU_SET(i,&shdata->resize_mask);
                CPU_CLR(i,&mask);
                set_new_mask(process, &mask, false );
                talp_cpu_disable(i);
                break;
            }
        }
    }
    else if( temp > 0.9){
        cpu_set_t mask;
        mask = process->current_process_mask;
        if(CPU_COUNT(&shdata->resize_mask) == 0) return DLB_SUCCESS;
        for(i = 0; i < max_cpus; ++i){
            if(CPU_ISSET(i,&shdata->resize_mask)){
                shmem_lock(shm_handler);
                if(!CPU_ISSET(i,&shdata->resize_mask)){
                    shmem_unlock(shm_handler);
                    continue;
                }
                CPU_SET(i,&mask);
                CPU_CLR(i, &shdata->resize_mask);
                shmem_unlock(shm_handler);
                set_new_mask(process,&mask, false );
                talp_cpu_enable(i);
                break;
            }
        }
    }
    return DLB_SUCCESS;
}

int auto_resize_end(){
    if ( !thread_spd->talp_info ) return 0 ;
    shmem_lock(shm_handler);
    {
        pinfo_t *process = get_process(thread_spd->id);
        monitor_info_t * monitor = &process->auto_sizer.aux;
        // FIXME: fix type
        /* monitoring_region_stop(monitor); */
        iter_t* it = &process->auto_sizer.iterations[process->auto_sizer.iter_num];
        it->mpi_time = monitor->mpi_time;
        it->compute_time = monitor->compute_time;
        it->percent = to_secs(it->compute_time) / ( to_secs(it->compute_time) + to_secs(it->mpi_time));
        ++process->auto_sizer.iter_num;
    }
    shmem_unlock(shm_handler);

    return DLB_SUCCESS;

}
