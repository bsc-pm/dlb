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
#include "LB_numThreads/numThreads.h"
#include "LB_comm/shmem_drom.h"
#include "support/options.h"
#include "support/debug.h"

void drom_init( void ) {
    shmem_drom__init();
}

void drom_finalize( void ) {
    shmem_drom__finalize();
}

void drom_update( void ) {
    shmem_drom__update();
}


void drom_ext_init( void ) {
    pm_init();
    options_init();
    debug_init();
    shmem_drom_ext__init();
}

void drom_ext_finalize( void ) {
    shmem_drom_ext__finalize();
    options_finalize();
}

int drom_ext_getnumcpus( void ) {
    return shmem_drom_ext__getnumcpus();
}

void drom_ext_getpidlist( int *pidlist, int *nelems, int max_len ) {
    shmem_drom_ext__getpidlist(pidlist, nelems, max_len);
}

int drom_ext_getprocessmask( int pid, cpu_set_t *mask ) {
    return shmem_drom_ext__getprocessmask( pid, mask );
}

int drom_ext_setprocessmask( int pid, const cpu_set_t *mask ) {
    return shmem_drom_ext__setprocessmask( pid, mask );
}

void drom_ext_printshmem(void) {
    shmem_drom_ext__print_info();
}
