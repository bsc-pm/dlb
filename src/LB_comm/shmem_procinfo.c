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
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/resource.h>

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_numThreads/numThreads.h"
#include "support/debug.h"
#include "support/types.h"
#include "support/options.h"
#include "support/mytime.h"
#include "support/mask_utils.h"

#define NOBODY 0
static const long UPDATE_USAGE_MIN_THRESHOLD    =  100000000L;   // 10^8 ns = 100ms
//static const long UPDATE_LOADAVG_MIN_THRESHOLD  = 1000000000L;   // 10^9 ns = 1s
//static const double LOG2E = 1.44269504088896340736;

typedef pid_t spid_t;  // Sub-process ID

typedef struct {
    spid_t pid;
    bool dirty;
    cpu_set_t current_process_mask;
    cpu_set_t future_process_mask;
    cpu_set_t stolen_cpus;
    unsigned int active_cpus;
    // Cpu Usage fields:
    double cpu_usage;
    double cpu_avg_usage;
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
    pinfo_t process_info[0];
} shdata_t;

static spid_t ME;
static shmem_handler_t *shm_handler = NULL;
static shdata_t *shdata = NULL;
static int max_cpus;
static int max_processes;
static int my_process = -1;
static struct timespec last_ttime; // Total time
static struct timespec last_utime; // Useful time (user+system)
static const char *shmem_name = "procinfo";

static void update_process_loads(void);
static void update_process_mask(void);
static bool steal_cpu(int cpu, int process, bool dry_run);
static int register_mask(const cpu_set_t *mask);
static void unregister_mask(const cpu_set_t *mask);
static int set_new_mask(int process, const cpu_set_t *mask, bool dry_run);
static int steal_mask(const cpu_set_t *mask, bool dry_run);

void shmem_procinfo__init(void) {
    // Protect double initialization
    if (shm_handler != NULL) {
        warning("Shared Memory is being initialized more than once");
        print_backtrace();
        return;
    }

    ME = getpid();

    // We will reserve enough positions assuming that there won't be more processes than cpus
    max_processes = mu_get_system_size();
    max_cpus = mu_get_system_size();

    // Basic size + zero-length array real length
    shmem_handler_t *init_handler = shmem_init((void**)&shdata,
            sizeof(shdata_t) + sizeof(pinfo_t)*max_processes, shmem_name);

    shmem_lock(init_handler);
    {
        // Initialize some values if this is the 1st process attached to the shmem
        if (!shdata->initialized) {
            get_time(&shdata->initial_time);
            mu_get_system_mask(&shdata->free_mask);
            shdata->initialized = true;
        }

        int p;
        for (p = 0; p < max_processes; p++) {
            // Register process
            if (shdata->process_info[p].pid == NOBODY) {
                shdata->process_info[p].pid = ME;
                shdata->process_info[p].dirty = false;

                // Get process mask and set current == future
                get_process_mask(&shdata->process_info[p].current_process_mask);
                memcpy(&shdata->process_info[p].future_process_mask,
                        &shdata->process_info[p].current_process_mask, sizeof(cpu_set_t));

                // Register mask into the system
                int error = register_mask(&shdata->process_info[p].current_process_mask);
                if (error) {
                    shmem_unlock(init_handler);
                    fatal("Error trying to register CPU mask: %s",
                        mu_to_str(&shdata->process_info[p].current_process_mask));
                }

#ifdef DLB_LOAD_AVERAGE
                shdata->process_info[p].load[0] = 0.0f;
                shdata->process_info[p].load[1] = 0.0f;
                shdata->process_info[p].load[2] = 0.0f;
#endif
                my_process = p;
                break;
            }
            // Process already registered
            else if (shdata->process_info[p].pid == ME) {
                my_process = p;
                break;
            }
        }
        if (p == max_processes) {
            shmem_unlock(init_handler);
            fatal("Not enough space in the shared memory to register process %d", ME);
        }
    }
    shmem_unlock(init_handler);

    // Global variable is only assigned after the initialization
    shm_handler = init_handler;
}

void shmem_procinfo__finalize(void) {
    shmem_lock(shm_handler);
    {
        // Unregister our process mask, or future mask if we are dirty
        if (shdata->process_info[my_process].dirty) {
            unregister_mask(&shdata->process_info[my_process].future_process_mask);
        } else {
            unregister_mask(&shdata->process_info[my_process].current_process_mask);
        }

        // Clear process fields
        shdata->process_info[my_process].pid = NOBODY;
        shdata->process_info[my_process].dirty = false;
        CPU_ZERO(&shdata->process_info[my_process].current_process_mask);
        CPU_ZERO(&shdata->process_info[my_process].future_process_mask);
        CPU_ZERO(&shdata->process_info[my_process].stolen_cpus);
        shdata->process_info[my_process].active_cpus = 0;
        shdata->process_info[my_process].cpu_usage = 0.0;
        shdata->process_info[my_process].cpu_avg_usage = 0.0;
#ifdef DLB_LOAD_AVERAGE
        shdata->process_info[my_process].load[3] = {0.0f, 0.0f, 0.0f};
        shdata->process_info[my_process].last_ltime = {0};
#endif
    }
    shmem_unlock(shm_handler);
    shmem_finalize(shm_handler);
    shm_handler = NULL;
}

void shmem_procinfo__update(bool do_drom, bool do_stats) {

    if (shm_handler == NULL || shdata == NULL) return;

    // Do not update loads if the elapsed total time is less than a predefined threshold
    struct timespec now;
    int64_t elapsed;
    get_time_coarse( &now );
    elapsed = timespec_diff( &last_ttime, &now );

    bool update_mask = do_drom && shdata->process_info[my_process].dirty;
    bool update_load = do_stats && elapsed >= UPDATE_USAGE_MIN_THRESHOLD;

    if (update_mask || update_load) {
        shmem_lock(shm_handler);
        {
            if (update_mask) {
                update_process_mask();
            }
            if (update_load) {
                update_process_loads();
            }
        }
        shmem_unlock(shm_handler);
    }
}

/* External Functions
 * These functions are intended to be called from external processes only to consult the shdata
 * That's why we should initialize and finalize the shared memory
 */

static shmem_handler_t *shm_ext_handler = NULL;

void shmem_procinfo_ext__init(void) {
    // Protect double initialization
    if (shm_ext_handler != NULL) {
        return;
    }

    max_cpus = mu_get_system_size();
    max_processes = mu_get_system_size();
    shm_ext_handler = shmem_init((void**)&shdata,
            sizeof(shdata_t) + sizeof(pinfo_t)*max_processes, shmem_name );

    shmem_lock(shm_ext_handler);
    {
        // Initialize some values if this is the 1st process attached to the shmem
        if (!shdata->initialized) {
            get_time(&shdata->initial_time);
            mu_get_system_mask(&shdata->free_mask);
            shdata->initialized = true;
        }
    }
    shmem_unlock(shm_ext_handler);
}

void shmem_procinfo_ext__finalize(void) {
    // Protect double finalization
    if (shm_ext_handler == NULL) {
        return;
    }

    shmem_finalize(shm_ext_handler);
    shm_ext_handler = NULL;
}

int shmem_procinfo_ext__preregister(int pid, const cpu_set_t *mask, int steal) {
    int error = 1;
    shmem_lock(shm_ext_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid == NOBODY) {
                shdata->process_info[p].pid = pid;
                shdata->process_info[p].dirty = false;

                // Register mask into the system
                error = steal ? set_new_mask(p, mask, false) : register_mask(mask);
                if (error) {
                    shmem_unlock(shm_ext_handler);
                    fatal("Error trying to register CPU mask: %s", mu_to_str(mask));
                }

                // Get process mask and set current == future
                memcpy(&shdata->process_info[p].current_process_mask, mask, sizeof(cpu_set_t));
                memcpy(&shdata->process_info[p].future_process_mask, mask, sizeof(cpu_set_t));

#ifdef DLB_LOAD_AVERAGE
                shdata->process_info[p].load[0] = 0.0f;
                shdata->process_info[p].load[1] = 0.0f;
                shdata->process_info[p].load[2] = 0.0f;
#endif
                break;
            }
        }
        if (p == max_processes) {
            shmem_unlock(shm_ext_handler);
            fatal("Not enough space in the shared memory to register process %d", pid);
        }
    }
    shmem_unlock(shm_ext_handler);
    return error;
}

void shmem_procinfo_ext__getpidlist(int *pidlist, int *nelems, int max_len) {
    *nelems = 0;
    if (shm_ext_handler == NULL) return;
    shmem_lock(shm_ext_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            int pid = shdata->process_info[p].pid;
            if (pid != NOBODY) {
                pidlist[(*nelems)++] = pid;
            }
            if (*nelems == max_len) {
                break;
            }
        }
    }
    shmem_unlock(shm_ext_handler);
}

int shmem_procinfo_ext__getprocessmask(int pid, cpu_set_t *mask) {
    if (shm_ext_handler == NULL) return -1;

    int error = -1;
    shmem_lock(shm_ext_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid == pid) {
                memcpy(mask, &(shdata->process_info[p].current_process_mask), sizeof(cpu_set_t));
                error = 0;
                break;
            }
        }
    }
    shmem_unlock(shm_ext_handler);
    return error;
}

int shmem_procinfo_ext__setprocessmask(int pid, const cpu_set_t *mask) {
    if (shm_ext_handler == NULL) return -1;

    int error = 0;
    shmem_lock(shm_ext_handler);
    {
        // Find process
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid == pid) {
                break;
            }
        }

        // PID not found
        if (p == max_processes) {
            error = -1;
        }

        // Run first a dry run to see if the mask can be completely stolen. If it's ok, run it.
        error = error ? error : set_new_mask(p, mask, true);
        error = error ? error : set_new_mask(p, mask, false);
        if (!error) {
            // If everything went ok, update future_mask and dirty flag
            memcpy(&shdata->process_info[p].future_process_mask, mask, sizeof(cpu_set_t));
            shdata->process_info[p].dirty = true;
        }
    }
    shmem_unlock(shm_ext_handler);
    return error;
}

double shmem_procinfo_ext__getcpuusage(int pid) {
    if (shm_ext_handler == NULL) return -1.0;

    double cpu_usage = -1.0;
    shmem_lock(shm_ext_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid == pid) {
                cpu_usage = shdata->process_info[p].cpu_usage;
                break;
            }
        }
    }
    shmem_unlock(shm_ext_handler);

    return cpu_usage;
}

double shmem_procinfo_ext__getcpuavgusage(int pid) {
    if (shm_ext_handler == NULL) return -1.0;

    double cpu_avg_usage = -1.0;
    shmem_lock(shm_ext_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid == pid) {
                cpu_avg_usage = shdata->process_info[p].cpu_avg_usage;
                break;
            }
        }
    }
    shmem_unlock(shm_ext_handler);

    return cpu_avg_usage;
}

void shmem_procinfo_ext__getcpuusage_list(double *usagelist, int *nelems, int max_len) {
    *nelems = 0;
    if (shm_ext_handler == NULL) return;
    shmem_lock(shm_ext_handler);
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
    shmem_unlock(shm_ext_handler);
}

void shmem_procinfo_ext__getcpuavgusage_list(double *avgusagelist, int *nelems, int max_len) {
    *nelems = 0;
    if (shm_ext_handler == NULL) return;
    shmem_lock(shm_ext_handler);
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
    shmem_unlock(shm_ext_handler);
}

double shmem_procinfo_ext__getnodeusage(void) {
    if (shm_ext_handler == NULL) return -1.0;

    double cpu_usage = 0.0;
    shmem_lock(shm_ext_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid != NOBODY) {
                cpu_usage += shdata->process_info[p].cpu_usage;
            }
        }
    }
    shmem_unlock(shm_ext_handler);

    return cpu_usage;
}

double shmem_procinfo_ext__getnodeavgusage(void) {
    if (shm_ext_handler == NULL) return -1.0;

    double cpu_avg_usage = 0.0;
    shmem_lock(shm_ext_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid != NOBODY) {
                cpu_avg_usage += shdata->process_info[p].cpu_avg_usage;
            }
        }
    }
    shmem_unlock(shm_ext_handler);

    return cpu_avg_usage;
}

int shmem_procinfo_ext__getactivecpus(int pid) {
    if (shm_ext_handler == NULL) return -1;

    int active_cpus = -1;
    shmem_lock(shm_ext_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid == pid) {
                active_cpus = shdata->process_info[p].active_cpus;
                break;
            }
        }
    }
    shmem_unlock(shm_ext_handler);
    return active_cpus;
}

void shmem_procinfo_ext__getactivecpus_list(int *cpuslist, int *nelems, int max_len) {
    *nelems = 0;
    if (shm_ext_handler == NULL) return;
    shmem_lock(shm_ext_handler);
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
    shmem_unlock(shm_ext_handler);
}

int shmem_procinfo_ext__getloadavg(int pid, double *load) {
    if (shm_ext_handler == NULL) return -1;
    int error = -1;
#ifdef DLB_LOAD_AVERAGE
    shmem_lock(shm_ext_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid == pid) {
                load[0] = shdata->process_info[p].load[0];
                load[1] = shdata->process_info[p].load[1];
                load[2] = shdata->process_info[p].load[2];
                error = 0;
                break;
            }
        }
    }
    shmem_unlock(shm_ext_handler);
#endif
    return error;
}

int shmem_procinfo_ext__getcpus(int ncpus, int steal, int *cpulist, int *nelems, int max_len) {
    *nelems = 0;
    if (shm_ext_handler == NULL) return -1;

    int n_elems = 0;
    shmem_lock(shm_ext_handler);
    {
        int c, p;
        // Get CPUs from the free_mask
        for (c = 0; c < max_cpus; c++) {
            if (CPU_ISSET(c, &shdata->free_mask)) {
                CPU_CLR(c, &shdata->free_mask);
                cpulist[n_elems++] = c;
            }
            if (n_elems == ncpus || n_elems == max_len) {
                break;
            }
        }

        // If 'steal' is enabled, get more CPUs from other processes in round-robin
        if (steal && n_elems < ncpus && n_elems < max_len) {
            while(true) {
                // We won't keep looping unless we found at least one candidate
                bool keep_looping = false;

                for (p = 0; p < max_processes; p++) {
                    if (shdata->process_info[p].pid != NOBODY) {
                        for (c = max_cpus-1; c >= 0; c--) {
                            bool success = steal_cpu(c, p, false);
                            if (success) {
                                // keep going from the next process
                                cpulist[n_elems++] = c;
                                keep_looping = true;
                                break;
                            }
                        }
                        if (n_elems == ncpus || n_elems == max_len) {
                            keep_looping = false;
                            break;
                        }
                    }
                }
                if (!keep_looping) break;
            }
        }
    }
    shmem_unlock(shm_ext_handler);

    *nelems = n_elems;
    return (n_elems == ncpus) ? 0 : -1;
}

void shmem_procinfo_ext__print_info(void) {
    if (shm_ext_handler == NULL) {
        warning("The shmem %s is not initialized, cannot print", shmem_name);
        return;
    }

    // Make a full copy of the shared memory. Basic size + zero-length array real length
    shdata_t *shdata_copy = (shdata_t*) malloc(sizeof(shdata_t) + sizeof(pinfo_t)*max_processes);

    shmem_lock(shm_ext_handler);
    {
        memcpy(shdata_copy, shdata, sizeof(shdata_t) + sizeof(pinfo_t)*max_processes);
    }
    shmem_unlock(shm_ext_handler);

    info0("=== Processes Masks ===");
    int p;
    for (p = 0; p < max_processes; p++) {
        if (shdata_copy->process_info[p].pid != NOBODY) {
            char current[max_processes*2+16];
            char future[max_processes*2+16];
            char stolen[max_processes*2+16];
            strncpy(current, mu_to_str(&shdata_copy->process_info[p].current_process_mask),
                    sizeof(current));
            strncpy(future, mu_to_str(&shdata_copy->process_info[p].future_process_mask),
                    sizeof(future));
            strncpy(stolen, mu_to_str(&shdata_copy->process_info[p].stolen_cpus),
                    sizeof(stolen));
            info0("PID: %d, Current: %s, Future: %s, Stolen: %s, Dirty: %d",
                    shdata_copy->process_info[p].pid, current, future, stolen,
                    shdata_copy->process_info[p].dirty);
        }
    }
    info0("=== Processes Statistics ===");
    for (p = 0; p < max_processes; p++) {
        if (shdata_copy->process_info[p].pid != NOBODY) {
            info0("Process %d: %f CPU Avg usage", p, shdata->process_info[p].cpu_avg_usage);
        }
    }

    free(shdata_copy);
}

/*** Helper functions, the shm lock must have been acquired beforehand ***/
static void update_process_loads(void) {
    // Get the active CPUs
    cpu_set_t mask;
    get_mask(&mask);
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
    elapsed_utime_since_init = ts_to_ns(&current_utime);

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

static void update_process_mask(void) {

    // Set up our next mask. We cannot blindly use the future_mask because the PM might reject it
    cpu_set_t *next_mask = &shdata->process_info[my_process].future_process_mask;

    // Notify the mask change to the PM
    verbose(VB_DROM, "Setting new mask: %s", mu_to_str(next_mask))
    int error = set_process_mask(next_mask);

    // On error, update local mask and steal again from other processes
    if (error) {
        get_process_mask(next_mask);
        int c, p;
        for (c = max_cpus-1; c >= 0; c--) {
            if (CPU_ISSET( c, next_mask) ) {
                for (p = 0; p < max_processes; p++) {
                    if (p == my_process ) continue;
                    // Steal CPU only if other process currently owns it
                    steal_cpu(c, p, false);
                }
            }
        }
    }
    // Update local info
    memcpy(&shdata->process_info[my_process].current_process_mask, next_mask, sizeof(cpu_set_t));
    shdata->process_info[my_process].dirty = false;
}

static bool steal_cpu(int cpu, int process, bool dry_run) {
    bool steal;

    // If not dirty, check that the CPU is owned by the process and it's not the last one
    steal = !shdata->process_info[process].dirty
        && CPU_ISSET(cpu, &shdata->process_info[process].current_process_mask)
        && CPU_COUNT(&shdata->process_info[process].future_process_mask) > 1;

    // If dirty, check the same but in the future mask
    steal |= shdata->process_info[process].dirty
        && CPU_ISSET(cpu, &shdata->process_info[process].future_process_mask)
        && CPU_COUNT(&shdata->process_info[process].future_process_mask) > 1;


    if (steal) {
        if (!dry_run) {
            shdata->process_info[process].dirty = true;
            CPU_SET(cpu, &shdata->process_info[process].stolen_cpus);
            CPU_CLR(cpu, &shdata->process_info[process].future_process_mask);

            // Add the stolen mask to the free_mask so other processes can acquire it
            CPU_SET(cpu, &shdata->free_mask);
        }
        verbose(VB_DROM, "CPU %d has been removed from process %d", cpu, process);
    }

    return steal;
}

// Register a new set of CPUs. Remove them from the free_mask
static int register_mask(const cpu_set_t *mask) {
    int c;
    for (c = 0; c < max_cpus; c++) {
        if (CPU_ISSET(c, mask)) {
            if (CPU_ISSET(c, &shdata->free_mask)) {
                CPU_CLR(c, &shdata->free_mask);
            } else {
                return -1;
            }
        }
    }
    return 0;
}

// Unregister CPUs. Either add them to the free_mask or give them back to their owner
//   * update: only return CPUs if it's enabled in debug options
static void unregister_mask(const cpu_set_t *mask) {
    verbose(VB_DROM, "Process %d unregistering mask %s", my_process, mu_to_str(mask));
    int c, p;
    for (c = 0; c < max_cpus; c++) {
        if (CPU_ISSET(c, mask)) {
            if (options_get_debug_opts() & DBG_RETURNSTOLEN) {
                // look if the CPU belongs to some other process
                for (p = 0; p < max_processes; p++) {
                    if (CPU_ISSET(c, &shdata->process_info[p].stolen_cpus)) {
                        // give it back to the process p
                        CPU_SET(c, &shdata->process_info[p].future_process_mask);
                        CPU_CLR(c, &shdata->process_info[p].stolen_cpus);
                        shdata->process_info[p].dirty = true;
                        verbose(VB_DROM, "Giving back CPU %d to process %d", c, p);
                        break;
                    }
                }

                // if we didn't find the owner, add it to the free_mask
                if (p == max_processes) {
                    CPU_SET(c, &shdata->free_mask);
                }
            } else {
                CPU_SET(c, &shdata->free_mask);
            }
        }
    }
}

// Configure a new cpu_set for the process
//  * If the CPU is SET and unused, remove it from the free_mask
//  * If the CPU is SET, used and not owned, try to steal it
//  * If the CPU is UNSET and owned by thr process, unregister it
// Returns true if all CPUS could be stolen
static int set_new_mask(int process, const cpu_set_t *mask, bool dry_run) {
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
                if (!CPU_ISSET(c, &shdata->process_info[process].future_process_mask)) {
                    // CPU is being used by other process
                    CPU_SET(c, &cpus_to_steal);
                }
            }
        } else {
            if (CPU_ISSET(c, &shdata->process_info[process].future_process_mask)) {
                // CPU no longer used by this process
                CPU_SET(c, &cpus_to_free);
            }
        }
    }

    int error = steal_mask(&cpus_to_steal, dry_run);

    if (!dry_run && !error) {
        register_mask(&cpus_to_acquire);
        unregister_mask(&cpus_to_free);
    }

    return error;
}

// Steal every CPU set in mask from other processes
static int steal_mask(const cpu_set_t *mask, bool dry_run) {
    int c, p;
    for (c = max_cpus-1; c >= 0; c--) {
        if (CPU_ISSET(c, mask)) {
            for (p = 0; p < max_processes; p++) {
                bool success = steal_cpu(c, p, dry_run);
                if (success) break;
            }

            if (p == my_process) {
                // No process returned a success
                verbose(VB_DROM, "CPU %d could not get acquired", c);
                return -1;
            }
        }
    }
    return 0;
}
