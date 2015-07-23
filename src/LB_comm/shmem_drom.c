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
#include "support/globals.h"
#include "support/tracing.h"

#define NOBODY 0
#define ME getpid()

typedef pid_t spid_t;  // Sub-process ID

typedef struct {
    cpu_set_t current_process_mask;
    cpu_set_t future_process_mask;
    spid_t pid;
    bool dirty;
} pinfo_t;

typedef struct {
    pinfo_t process_info[0];
} shdata_t;

static shmem_handler_t *shm_handler = NULL;
static shmem_handler_t *shm_ext_handler = NULL;
static shdata_t *shdata = NULL;
static int max_cpus;
static int max_processes;
static int my_process;

static bool steal_cpu(int, int);

void shmem_drom__init(void) {
    // Protect double initialization
    if ( shm_handler != NULL ) {
        warning("DLB_Drom is being initialized more than once");
        return;
    }

    // We will reserve enough positions assuming that there won't be more processes than cpus
    max_processes = mu_get_system_size();
    max_cpus = mu_get_system_size();

    // Basic size + zero-length array real length
    shm_handler = shmem_init( (void**)&shdata, sizeof(shdata_t) + sizeof(pinfo_t)*max_processes, "drom" );

    shmem_lock( shm_handler );
    {
        int p;
        for ( p = 0; p < max_processes; p++ ) {
            if ( shdata->process_info[p].pid == NOBODY ) {
                get_process_mask( &(shdata->process_info[p].current_process_mask) );
                shdata->process_info[p].pid = ME;
                shdata->process_info[p].dirty = false;
                my_process = p;
                break;
            }
        }
        fatal_cond( p == max_processes, "Cannot reserve process info for the Dynamic Resource Ownership Manager" );
    }
    shmem_unlock( shm_handler );
    _mpi_rank = ME;
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

    if (shm_handler == NULL || shdata == NULL) return;

    // If dirty, we will check again once locked
    if ( shdata->process_info[my_process].dirty ) {
        shmem_lock( shm_handler );
        {
            if ( shdata->process_info[my_process].dirty ) {
                // XXX: Should we do this step?
                // Update current process mask (it could have externally changed, although rare)
                get_process_mask( &(shdata->process_info[my_process].current_process_mask) );

                // Set up our next mask. We cannot blindly use the future_mask becasue some CPU might not be stolen
                cpu_set_t next_mask;
                memcpy( &next_mask, &(shdata->process_info[my_process].future_process_mask), sizeof(cpu_set_t) );

                // foreach set cpu in future mask, steal it from others unless it is the last one
                int c, p;
                for ( c = max_cpus-1; c >= 0; c-- ) {
                    if ( CPU_ISSET( c, &(shdata->process_info[my_process].future_process_mask) ) ) {
                        for ( p = 0; p < max_processes; p++ ) {
                            if ( p == my_process ) continue;
                            if ( CPU_ISSET( c, &(shdata->process_info[p].current_process_mask) )
                                    && CPU_COUNT( &(shdata->process_info[p].current_process_mask) ) > 1 ) {
                                // Steal CPU only if other process currently owns it
                                if ( !steal_cpu(c, p) ) {
                                    // If stealing was not successfull, do not set CPU
                                    CPU_CLR( p, &next_mask );
                                }
                            }
                        }
                    }
                }
                // Clear dirty flag only if we didn't clear any CPU in next_mask
                if ( CPU_EQUAL( &(shdata->process_info[my_process].future_process_mask), &next_mask ) ) {
                    shdata->process_info[my_process].dirty = false;
                }
                // Set final mask
                int error = set_process_mask( &next_mask );
                // On error, update local mask and steal again from other processes
                if ( error ) {
                    get_process_mask( &next_mask );
                    for ( c = max_cpus-1; c >= 0; c-- ) {
                        if ( CPU_ISSET( c, &next_mask ) ) {
                            for ( p = 0; p < max_processes; p++ ) {
                                if ( p == my_process ) continue;
                                if ( CPU_ISSET( c, &(shdata->process_info[p].current_process_mask) )
                                        && CPU_COUNT( &(shdata->process_info[p].current_process_mask) ) > 1 ) {
                                    // Steal CPU only if other process currently owns it
                                    steal_cpu(c, p);
                                }
                            }
                        }
                    }
                }
                // Update local info
                memcpy( &(shdata->process_info[my_process].current_process_mask), &next_mask, sizeof(cpu_set_t) );
            }
        }
        shmem_unlock( shm_handler );
    }
}

/* PRE: shmem_lock'd */
static bool steal_cpu( int cpu, int processor ) {
    if ( shdata->process_info[processor].dirty ) {
        // If the future mask is dirty, make sure that it is not the last one, else remove it
        if ( CPU_COUNT( &(shdata->process_info[processor].future_process_mask) ) == 1
                && CPU_ISSET( cpu, &(shdata->process_info[processor].future_process_mask) ) ) {
            // cannot steal, fallback
            return false;
        } else {
            CPU_CLR( cpu, &(shdata->process_info[processor].future_process_mask) );
        }
    } else {
        // If not dirty, set up a new mask
        memcpy( &(shdata->process_info[processor].future_process_mask),
                &(shdata->process_info[processor].current_process_mask),
                sizeof(cpu_set_t) );
        CPU_CLR( cpu, &(shdata->process_info[processor].future_process_mask) );
        shdata->process_info[processor].dirty = true;
    }
    return true;
}

/* From here: functions aimed to be called from an external process */

void shmem_drom_ext__init( void ) {
    // Protect double initialization
    if ( shm_ext_handler != NULL ) {
        warning("DLB_Stats is being initialized more than once");
        return;
    }

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

int shmem_drom_ext__getprocessmask( int pid, cpu_set_t *mask ) {
    int error = -1;
    shmem_lock( shm_ext_handler );
    {
        int p;
        for ( p = 0; p < max_processes; p++ ) {
            if ( shdata->process_info[p].pid == pid ) {
                memcpy( mask, &(shdata->process_info[p].current_process_mask), sizeof(cpu_set_t) );
                error = 0;
                break;
            }
        }
    }
    shmem_unlock( shm_ext_handler );
    return error;
}

int shmem_drom_ext__setprocessmask( int pid, const cpu_set_t *mask ) {
    int error = -1;
    shmem_lock( shm_ext_handler );
    {
        int p;
        for ( p = 0; p < max_processes; p++ ) {
            if ( shdata->process_info[p].pid == pid ) {
                memcpy( &(shdata->process_info[p].future_process_mask), mask, sizeof(cpu_set_t) );
                shdata->process_info[p].dirty = true;
                error = 0;
                break;
            }
        }
    }
    shmem_unlock( shm_ext_handler );
    return error;
}


/* This function is intended to be called from external processes only to consult the shdata
 * That's why we should initialize and finalize the shared memory
 */
void shmem_drom_ext__printinfo( void ) {
    max_processes = mu_get_system_size();
    //
    // Basic size + zero-length array real length
    shmem_handler_t *handler;
    handler = shmem_init( (void**)&shdata, sizeof(shdata_t) + sizeof(pinfo_t)*max_processes, "drom" );
    pinfo_t process_info_copy[max_processes];

    shmem_lock( handler );
    {
        memcpy( process_info_copy, shdata->process_info, sizeof(pinfo_t)*max_processes );
    }
    shmem_unlock( handler );
    shmem_finalize( handler );

    int p;
    for ( p = 0; p < max_processes; p++ ) {
        if ( process_info_copy[p].pid != NOBODY ) {
            char current[max_processes*2+16];
            char future[max_processes*2+16];
            strncpy( current, mu_to_str( &(process_info_copy[p].current_process_mask) ), sizeof(current) );
            strncpy( future, mu_to_str( &(process_info_copy[p].future_process_mask) ), sizeof(future) );

            verbose( VB_DROM, "PID: %d, Current mask: %s, Future mask: %s, Dirty: %d",
                    process_info_copy[p].pid, current, future, process_info_copy[p].dirty );
        }
    }
}
