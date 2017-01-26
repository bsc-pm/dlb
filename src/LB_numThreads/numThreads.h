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

#ifndef NUMTHREADS_H
#define NUMTHREADS_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>

void pm_init(void);
int update_threads(int threads);
int get_mask(cpu_set_t *cpu_set);
int set_mask(const cpu_set_t *cpu_set);
int add_mask(const cpu_set_t *cpu_set);
int enable_cpu(int cpuid);
int disable_cpu(int cpuid);
int get_process_mask(cpu_set_t *cpu_set);
int set_process_mask(const cpu_set_t *cpu_set);
int add_process_mask(const cpu_set_t *cpu_set);

#endif //NUMTHREADS_H
