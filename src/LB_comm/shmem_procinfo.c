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

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/resource.h>

#include "LB_comm/shmem.h"
#include "LB_comm/shmem_procinfo.h"
#include "LB_numThreads/numThreads.h"
#include "support/debug.h"
#include "support/mytime.h"
#include "support/mask_utils.h"

#define NOBODY 0
static const long UPDATE_USAGE_MIN_THRESHOLD    =  100000000L;   // 10^8 ns = 100ms
static const long UPDATE_LOADAVG_MIN_THRESHOLD  = 1000000000L;   // 10^9 ns = 1s
static const double LOG2E = 1.44269504088896340736;

typedef pid_t spid_t;  // Sub-process ID

typedef struct {
    spid_t pid;
    bool dirty;
    cpu_set_t current_process_mask;
    cpu_set_t future_process_mask;
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
    struct timespec initial_time;
    pinfo_t process_info[0];
} shdata_t;

static spid_t ME;
static shmem_handler_t *shm_handler = NULL;
static shdata_t *shdata = NULL;
static int max_processes;
static int my_process;

struct timespec last_ttime; // Total time
struct timespec last_utime; // Useful time (user+system)

static void update_process_loads(void);
static void update_process_mask(void);
static bool steal_cpu(int cpu, int processor);

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

    // Basic size + zero-length array real length
    shmem_handler_t *init_handler = shmem_init((void**)&shdata,
            sizeof(shdata_t) + sizeof(pinfo_t)*max_processes, "procinfo");

    shmem_lock(init_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid == NOBODY) {
                shdata->process_info[p].pid = ME;
                shdata->process_info[p].dirty = false;
                get_process_mask( &(shdata->process_info[p].current_process_mask) );
#ifdef DLB_LOAD_AVERAGE
                shdata->process_info[p].load[0] = 0.0f;
                shdata->process_info[p].load[1] = 0.0f;
                shdata->process_info[p].load[2] = 0.0f;
#endif
                my_process = p;
                break;
            }
        }
        fatal_cond(p == max_processes, "Cannot reserve process statistics");

        // Initialize initial time
        if (shdata->initial_time.tv_sec == 0 && shdata->initial_time.tv_nsec == 0) {
            get_time(&shdata->initial_time);
        }
    }
    shmem_unlock(init_handler);

    // Global variable is only assigned after the initialization
    shm_handler = init_handler;
}

void shmem_procinfo__finalize(void) {
    shmem_lock(shm_handler);
    {
        shdata->process_info[my_process].pid = NOBODY;
        shdata->process_info[my_process].dirty = false;
    }
    shmem_unlock(shm_handler);
    shmem_finalize(shm_handler);
    shm_handler = NULL;
}

void shmem_procinfo__update(void) {

    if (shm_handler == NULL || shdata == NULL) return;

    // Do not update loads if the elapsed total time is less than a predefined threshold
    struct timespec now;
    int64_t elapsed;
    get_time_coarse( &now );
    elapsed = timespec_diff( &last_ttime, &now );

    bool update_mask = shdata->process_info[my_process].dirty;
    bool update_load = elapsed >= UPDATE_USAGE_MIN_THRESHOLD;

    if (update_mask || update_load) {
        shmem_lock(shm_handler);
        {
            // double check shdata once locked
            if (shdata->process_info[my_process].dirty) {
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

    max_processes = mu_get_system_size();
    shm_ext_handler = shmem_init((void**)&shdata,
            sizeof(shdata_t) + sizeof(pinfo_t)*max_processes, "procinfo" );
}

void shmem_procinfo_ext__finalize(void) {
    shmem_finalize(shm_ext_handler);
    shm_ext_handler = NULL;
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

    int error = -1;
    shmem_lock(shm_ext_handler);
    {
        int p;
        for (p = 0; p < max_processes; p++) {
            if (shdata->process_info[p].pid == pid) {
                memcpy(&(shdata->process_info[p].future_process_mask), mask, sizeof(cpu_set_t));
                shdata->process_info[p].dirty = true;
                error = 0;
                break;
            }
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

void shmem_procinfo_ext__print_info(void) {
    max_processes = mu_get_system_size();

    // Basic size + zero-length array real length
    shmem_handler_t *handler = shmem_init((void**)&shdata,
            sizeof(shdata_t) + sizeof(pinfo_t)*max_processes, "procinfo");
    pinfo_t process_info_copy[max_processes];

    shmem_lock(handler);
    {
        memcpy(process_info_copy, shdata->process_info, sizeof(pinfo_t)*max_processes);
    }
    shmem_unlock(handler);
    shmem_finalize(handler);

    int p;
    for (p = 0; p < max_processes; p++) {
        if (process_info_copy[p].pid != NOBODY) {
            char current[max_processes*2+16];
            char future[max_processes*2+16];
            strncpy(current, mu_to_str(&(process_info_copy[p].current_process_mask)),
                    sizeof(current));
            strncpy(future, mu_to_str(&(process_info_copy[p].future_process_mask)),
                    sizeof(future));
            fprintf(stdout, "PID: %d, Current mask: %s, Future mask: %s, Dirty: %d\n",
                    process_info_copy[p].pid, current, future, process_info_copy[p].dirty);
        }
    }
    fprintf(stdout, "=== DLB Statistics ===\n");
    fprintf(stdout, "Process %d: %f CPU Avg usage\n",
            my_process, shdata->process_info[my_process].cpu_avg_usage);

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
    int max_cpus = mu_get_system_size();
    // XXX: Should we do this step?
    // Update current process mask (it could have externally changed, although rare)
    get_process_mask(&(shdata->process_info[my_process].current_process_mask));

    // Set up our next mask. We cannot blindly use the future_mask becasue
    // some CPU might not be stolen
    cpu_set_t next_mask;
    memcpy(&next_mask, &(shdata->process_info[my_process].future_process_mask), sizeof(cpu_set_t));

    // foreach set cpu in future mask, steal it from others unless it is the last one
    int c, p;
    for (c = max_cpus-1; c >= 0; c--) {
        if (CPU_ISSET(c, &(shdata->process_info[my_process].future_process_mask))) {
            for (p = 0; p < max_processes; p++) {
                if (p == my_process) continue;
                if (CPU_ISSET( c, &(shdata->process_info[p].current_process_mask))
                        && CPU_COUNT(&(shdata->process_info[p].current_process_mask)) > 1) {
                    // Steal CPU only if other process currently owns it
                    if (!steal_cpu(c, p)) {
                        // If stealing was not successfull, do not set CPU
                        CPU_CLR(p, &next_mask);
                    }
                }
            }
        }
    }
    // Clear dirty flag only if we didn't clear any CPU in next_mask
    if (CPU_EQUAL(&(shdata->process_info[my_process].future_process_mask), &next_mask)) {
        shdata->process_info[my_process].dirty = false;
    }
    // Set final mask
    verbose(VB_DROM, "Setting new mask: %s", mu_to_str(&next_mask));
    int error = set_process_mask(&next_mask);
    // On error, update local mask and steal again from other processes
    if (error) {
        get_process_mask(&next_mask);
        for (c = max_cpus-1; c >= 0; c--) {
            if (CPU_ISSET( c, &next_mask) ) {
                for (p = 0; p < max_processes; p++) {
                    if (p == my_process ) continue;
                    if (CPU_ISSET(c, &(shdata->process_info[p].current_process_mask))
                            && CPU_COUNT(&(shdata->process_info[p].current_process_mask)) > 1) {
                        // Steal CPU only if other process currently owns it
                        steal_cpu(c, p);
                    }
                }
            }
        }
    }
    // Update local info
    memcpy(&(shdata->process_info[my_process].current_process_mask), &next_mask, sizeof(cpu_set_t));
}

static bool steal_cpu(int cpu, int processor) {
    if (shdata->process_info[processor].dirty) {
        // If the future mask is dirty, make sure that it is not the last one, else remove it
        if (CPU_COUNT(&(shdata->process_info[processor].future_process_mask)) == 1
                && CPU_ISSET(cpu, &(shdata->process_info[processor].future_process_mask))) {
            // cannot steal, fallback
            return false;
        } else {
            CPU_CLR(cpu, &(shdata->process_info[processor].future_process_mask));
        }
    } else {
        // If not dirty, set up a new mask
        memcpy(&(shdata->process_info[processor].future_process_mask),
                &(shdata->process_info[processor].current_process_mask),
                sizeof(cpu_set_t));
        CPU_CLR(cpu, &(shdata->process_info[processor].future_process_mask));
        shdata->process_info[processor].dirty = true;
        verbose(VB_DROM, "Process %d stole CPU %d", processor, cpu);
    }
    return true;
}
