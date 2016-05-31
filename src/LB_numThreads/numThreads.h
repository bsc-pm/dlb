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
void update_threads(int threads);
void get_mask(cpu_set_t *cpu_set);
int  set_mask(const cpu_set_t *cpu_set);
void add_mask(const cpu_set_t *cpu_set);
void get_process_mask(cpu_set_t *cpu_set);
int  set_process_mask(const cpu_set_t *cpu_set);
void add_process_mask(const cpu_set_t *cpu_set);
int  get_thread_num(void);

#endif //NUMTHREADS_H
