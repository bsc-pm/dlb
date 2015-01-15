/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the DLB library.                                        */
/*                                                                                   */
/*      DLB is free software: you can redistribute it and/or modify                  */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      DLB is distributed in the hope that it will be useful,                       */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*************************************************************************************/

#define _GNU_SOURCE
#include <sched.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>

#include "LB_comm/shmem.h"
#include "LB_numThreads/numThreads.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/mytime.h"

#define NOBODY 0
#define ME getpid()
static const long UPDATE_USAGE_MIN_THRESHOLD = 100000L;         // 100ms
static const long UPDATE_LOADAVG_MIN_THRESHOLD = 1000000L;      // 1s
static const double LOG2E = 1.44269504088896340736;

typedef pid_t spid_t;  // Sub-process ID

typedef struct {
    spid_t pid;
    unsigned int active_cpus;
    // Cpu Usage fields:
    double cpu_usage;
    struct timeval last_ttime;  // Total time
    struct timeval last_utime;  // Useful time (user+system)
#ifdef DLB_LOAD_AVERAGE
    // Load average fields:
    float load[3];              // 1min, 5min, 15mins
    struct timeval last_ltime;  // Last time that Load was updated
#endif
} pinfo_t;

typedef struct {
    pinfo_t process_info[0];
} shdata_t;

static shmem_handler_t *shm_handler;
static shdata_t *shdata;
static int max_processes;
static int my_process;

void shmem_stats__init( void ) {
    // We will reserve enough positions assuming that there won't be more processes than cpus
    max_processes = mu_get_system_size();

    // Basic size + zero-length array real length
    shm_handler = shmem_init( (void**)&shdata, sizeof(shdata_t) + sizeof(pinfo_t)*max_processes, "stats" );

    shmem_lock( shm_handler );
    {
        int p;
        for ( p = 0; p < max_processes; p++ ) {
            if ( shdata->process_info[p].pid == NOBODY ) {
                shdata->process_info[p].pid = ME;
#ifdef DLB_LOAD_AVERAGE
                shdata->process_info[p].load[0] = 0.0f;
                shdata->process_info[p].load[1] = 0.0f;
                shdata->process_info[p].load[2] = 0.0f;
#endif
                my_process = p;
                break;
            }
        }
        fatal_cond( p == max_processes, "Cannot reserve process statistics" );
    }
    shmem_unlock( shm_handler );
}

void shmem_stats__finalize( void ) {
    shmem_lock( shm_handler );
    {
        shdata->process_info[my_process].pid = NOBODY;
    }
    shmem_unlock( shm_handler );
    shmem_finalize( shm_handler );
}

void shmem_stats__update( void ) {

    // Do not update if the elapsed total time is less than a predefined threshold
    struct timeval time;
    long long elapsed;
    gettimeofday( &time, NULL );
    elapsed = timeval_diff( &shdata->process_info[my_process].last_ttime, &time );
    if ( elapsed < UPDATE_USAGE_MIN_THRESHOLD )
        return;

    shmem_lock( shm_handler );
    {
        // Get the active CPUs
        cpu_set_t mask;
        get_mask( &mask );
        shdata->process_info[my_process].active_cpus = CPU_COUNT( &mask );

        // Compute elapsed total time
        struct timeval current_ttime;
        long long elapsed_ttime;
        gettimeofday( &current_ttime, NULL );
        elapsed_ttime = timeval_diff( &shdata->process_info[my_process].last_ttime, &current_ttime );

        // Compute elapsed useful time (user+system)
        struct rusage usage;
        struct timeval current_utime;
        long long elapsed_utime;
        getrusage( RUSAGE_SELF, &usage );
        timeradd( &usage.ru_utime, &usage.ru_stime, &current_utime );
        elapsed_utime = timeval_diff( &shdata->process_info[my_process].last_utime, &current_utime );

        // Update times for next update
        shdata->process_info[my_process].last_ttime = current_ttime;
        shdata->process_info[my_process].last_utime = current_utime;

        // Compute and store usage value
        double cpu_usage = 100 * (double)elapsed_utime / (double)elapsed_ttime;
        shdata->process_info[my_process].cpu_usage = cpu_usage;

#ifdef DLB_LOAD_AVERAGE
        // Do not update the Load Average if the elapsed is less that a threshold
        if ( elapsed_ttime > UPDATE_LOADAVG_MIN_THRESHOLD ) {
            shdata->process_info[my_process].last_ltime = current_ttime;
            // WIP
        }
#endif
    }
    shmem_unlock( shm_handler );
}


/* From here: functions aimed to be called from an external process, only to consult the shmem */

void shmem_stats_ext__init( void ) {
    max_processes = mu_get_system_size();
    shm_handler = shmem_init( (void**)&shdata, sizeof(shdata_t) + sizeof(pinfo_t)*max_processes, "stats" );
}

void shmem_stats_ext__finalize( void ) {
    shmem_finalize( shm_handler );
}

int shmem_stats_ext__getnumcpus( void ) {
    return mu_get_system_size();
}

void shmem_stats_ext__getpidlist( int *pidlist, int *nelems, int max_len ) {
    *nelems = 0;
    shmem_lock( shm_handler );
    {
        int p;
        for ( p = 0; p < max_processes; p++ ) {
            int pid = shdata->process_info[p].pid;
            if ( pid != NOBODY ) {
                pidlist[(*nelems)++] = pid;
            }
            if ( *nelems == max_len ) {
                break;
            }
        }
    }
    shmem_unlock( shm_handler );
}

double shmem_stats_ext__getcpuusage( int pid ) {
    double cpu_usage = 0.0;

    shmem_lock( shm_handler );
    {
        int p;
        for ( p = 0; p < max_processes; p++ ) {
            if ( shdata->process_info[p].pid == pid ) {
                cpu_usage = shdata->process_info[p].cpu_usage;
                break;
            }
        }
    }
    shmem_unlock( shm_handler );

    return cpu_usage;
}

void shmem_stats_ext__getcpuusage_list( double *usagelist, int *nelems, int max_len ) {
    *nelems = 0;
    shmem_lock( shm_handler );
    {
        int p;
        for ( p = 0; p < max_processes; p++ ) {
            if ( shdata->process_info[p].pid != NOBODY ) {
                usagelist[(*nelems)++] = shdata->process_info[p].cpu_usage;
            }
            if ( *nelems == max_len ) {
                break;
            }
        }
    }
    shmem_unlock( shm_handler );
}

int shmem_stats_ext__getactivecpus( int pid ) {
    int active_cpus = 0;
    shmem_lock( shm_handler );
    {
        int p;
        for ( p = 0; p < max_processes; p++ ) {
            if ( shdata->process_info[p].pid == pid ) {
                active_cpus = shdata->process_info[p].active_cpus;
                break;
            }
        }
    }
    shmem_unlock( shm_handler );
    return active_cpus;
}

void shmem_stats_ext__getactivecpus_list( int *cpuslist, int *nelems, int max_len ) {
    *nelems = 0;
    shmem_lock( shm_handler );
    {
        int p;
        for ( p = 0; p < max_processes; p++ ) {
            if ( shdata->process_info[p].pid != NOBODY ) {
                cpuslist[(*nelems)++] = shdata->process_info[p].active_cpus;
            }
            if ( *nelems == max_len ) {
                break;
            }
        }
    }
    shmem_unlock( shm_handler );
}

void shmem_stats_ext__getloadavg( int pid, double *load ) {
#ifdef DLB_LOAD_AVERAGE
    shmem_lock( shm_handler );
    {
        int p;
        for ( p = 0; p < max_processes; p++ ) {
            if ( shdata->process_info[p].pid == pid ) {
                load[0] = shdata->process_info[p].load[0];
                load[1] = shdata->process_info[p].load[1];
                load[2] = shdata->process_info[p].load[2];
                break;
            }
        }
    }
    shmem_unlock( shm_handler );
#endif
}
