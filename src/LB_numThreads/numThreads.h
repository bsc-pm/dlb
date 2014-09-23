/*************************************************************************************/
/*      Copyright 2012 Barcelona Supercomputing Center                               */
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

#ifndef NUMTHREADS_H
#define NUMTHREADS_H

#define _GNU_SOURCE
#include <sched.h>

void omp_set_num_threads (int numThreads) __attribute__ ((weak));
void css_set_num_threads (int numThreads) __attribute__ ((weak));
int* css_set_num_threads_cpus(int action, int num_cpus, int* cpus) __attribute__ ((weak));

void update_threads(int threads);
int* update_cpus(int action, int num_cpus, int* cpus);

void bind_master(int meId, int nodeId);
void DLB_bind_thread(int tid, int procsNode);
void bind_threads(int num_procs, int meId, int nodeId);

void get_mask( cpu_set_t *cpu_set );
void set_mask( const cpu_set_t *cpu_set );
void add_mask( const cpu_set_t *cpu_set );

#endif //NUMTHREADS_H
