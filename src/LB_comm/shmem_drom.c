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
#include <string.h>
#include <sys/types.h>

#include "LB_comm/shmem.h"
#include "LB_numThreads/numThreads.h"
#include "support/debug.h"
#include "support/utils.h"
#include "support/mask_utils.h"

#define NOBODY 0
#define ME getpid()

typedef pid_t spid_t;  // Sub-process ID

typedef struct {
    spid_t pid;
    cpu_set_t process_mask;
    bool dirty;
} pinfo_t;

typedef struct {
    pinfo_t process_info[0];
} shdata_t;

static shmem_handler_t *shm_handler;
static shmem_handler_t *shm_ext_handler;
static shdata_t *shdata;
static int max_processes;
static int my_process;

void shmem_drom__init(void) {
    // We will reserve enough positions assuming that there won't be more processes than cpus
    max_processes = mu_get_system_size();

    // Basic size + zero-length array real length
    shm_handler = shmem_init( (void**)&shdata, sizeof(shdata_t) + sizeof(pinfo_t)*max_processes, "drom" );

    shmem_lock( shm_handler );
    {
        int p;
        for ( p = 0; p < max_processes; p++ ) {
            if ( shdata->process_info[p].pid == NOBODY ) {
                shdata->process_info[p].pid = ME;
                CPU_ZERO( &(shdata->process_info[p].process_mask) );
                shdata->process_info[p].dirty = false;
                my_process = p;
                break;
            }
        }
        fatal_cond( p == max_processes, "Cannot reserve process info for the Dynamic Resource Ownership Manager" );
    }
    shmem_unlock( shm_handler );
}

void shmem_drom__finalize( void ) {
    shmem_lock( shm_handler );
    {
        shdata->process_info[my_process].pid = NOBODY;
        shdata->process_info[my_process].dirty = false;
    }
    shmem_unlock( shm_handler );
    shmem_finalize( shm_handler );
}

void shmem_drom__update( void ) {
    // If dirty, we will check again once locked
    if ( shdata->process_info[my_process].dirty ) {
        shmem_lock( shm_handler );
        {
            if ( shdata->process_info[my_process].dirty ) {
                set_process_mask( &(shdata->process_info[my_process].process_mask) );
            }
        }
        shmem_unlock( shm_handler );
    }
}

/* From here: functions aimed to be called from an external process */

void shmem_drom_ext__init( void ) {
    max_processes = mu_get_system_size();
    shm_ext_handler = shmem_init( (void**)&shdata, sizeof(shdata_t) + sizeof(pinfo_t)*max_processes, "drom" );
}

void shmem_drom_ext__finalize( void ) {
    shmem_finalize( shm_ext_handler );
}

int shmem_drom_ext__getnumcpus( void ) {
    return mu_get_system_size();
}

void shmem_drom_ext__getpidlist( int *pidlist, int *nelems, int max_len ) {
    *nelems = 0;
    shmem_lock( shm_ext_handler );
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
    shmem_unlock( shm_ext_handler );
}

void shmem_drom_ext__setprocessmask( int pid, cpu_set_t *mask ) {
    shmem_lock( shm_ext_handler );
    {
        int p;
        for ( p = 0; p < max_processes; p++ ) {
            if ( shdata->process_info[p].pid == pid ) {
                memcpy( &(shdata->process_info[my_process].process_mask), mask, sizeof(cpu_set_t) );
                shdata->process_info[my_process].dirty = true;
                break;
            }
        }
    }
    shmem_unlock( shm_ext_handler );
}
