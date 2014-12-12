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

#include "LB_comm/shmem_stats.h"

void stats_init( void ) {
    shmem_stats__init();
}

void stats_finalize( void ) {
    shmem_stats__finalize();
}

void stats_update( void ) {
    shmem_stats__update();
}

void stats_ext_init( void ) {
    shmem_stats_ext__init();
}

void stats_ext_finalize( void ) {
    shmem_stats_ext__finalize();
}

int stats_ext_getnumcpus( void ) {
    return shmem_stats_ext__getnumcpus();
}

void stats_ext_getpidlist( int *pidlist, int *nelems, int max_len ) {
    shmem_stats_ext__getpidlist( pidlist, nelems, max_len );
}

double stats_ext_getcpuusage( int pid ) {
    return shmem_stats_ext__getcpuusage( pid );
}

void stats_ext_getcpuusage_list( double *usagelist, int *nelems, int max_len ) {
    shmem_stats_ext__getcpuusage_list( usagelist, nelems, max_len );
}

int stats_ext_getactivecpus( int pid ) {
    return shmem_stats_ext__getactivecpus( pid );
}

void stats_ext_getactivecpus_list( int *cpuslist, int *nelems, int max_len) {
    shmem_stats_ext__getactivecpus_list(cpuslist, nelems, max_len);
}

void stats_ext_getloadavg( int pid, double *load ) {
    shmem_stats_ext__getloadavg( pid, load );
}
