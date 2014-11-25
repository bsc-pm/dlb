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

#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/resource.h>

#include "LB_comm/shmem.h"
#include "support/debug.h"
#include "support/mask_utils.h"
#include "support/mytime.h"

#define NOBODY 0
#define ME getpid()

typedef pid_t spid_t;  // Sub-process ID

typedef struct {
    spid_t pid;
    double cpu_usage;
    struct timeval last_ttime;  // Total time
    struct timeval last_utime;  // Useful time (user+system)
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
    shmem_lock( shm_handler );
    {
        // Compute elapsed total time
        struct timeval current_ttime;
        long long elapsed_ttime;
        gettimeofday( &current_ttime, NULL );
        elapsed_ttime = timeval_diff( &shdata->process_info[my_process].last_ttime, &current_ttime );

        // Compute elapsed useful time
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
    }
    shmem_unlock( shm_handler );
}


/* From here: functions aimed to be called from an external process, only to consult the shmem */

void shmem_stats_ext__init( void ) {
    mu_init();
    max_processes = mu_get_system_size();
    shm_handler = shmem_init( (void**)&shdata, sizeof(shdata_t) + sizeof(pinfo_t)*max_processes, "stats" );
}

double shmem_stats_ext__getcpuusage(int pid) {
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

void shmem_stats_ext__finalize( void ) {
    shmem_finalize( shm_handler );
}
