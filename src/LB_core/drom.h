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

#ifndef DROM_H
#define DROM_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>

void drom_ext_init(void);
void drom_ext_finalize(void);
int drom_ext_getnumcpus(void);
void drom_ext_getpidlist(int *pidlist, int *nelems, int max_len);
int drom_ext_getprocessmask(int pid, cpu_set_t *mask);
int drom_ext_setprocessmask(int pid, const cpu_set_t *mask);
int drom_ext_getprocessmask_sync(int pid, cpu_set_t *mask);
int drom_ext_setprocessmask_sync(int pid, const cpu_set_t *mask);
int drom_ext_getcpus(int ncpus, int steal, int *cpulist, int *nelems, int max_len);
int drom_ext_preregister(int pid, const cpu_set_t *mask, int steal);

#endif /* DROM_H */
