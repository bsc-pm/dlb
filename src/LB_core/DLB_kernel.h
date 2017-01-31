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

#ifndef DLB_KERNEL_H
#define DLB_KERNEL_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>
#include <stdbool.h>
#include "spd.h"

void set_dlb_enabled(bool enabled);

int Initialize(const cpu_set_t *mask, const char *lb_args);

int Finish(void);

void Update(void);

void IntoCommunication(void);

void OutOfCommunication(void);

void IntoBlockingCall(int is_iter, int is_single);

void OutOfBlockingCall(int is_iter);

void updateresources(int max_resources);

void returnclaimed(void);

int releasecpu(int cpu);

int returnclaimedcpu(int cpu);

void claimcpus(int cpus);

int checkCpuAvailability(int cpu);

void resetDLB(void);

void acquirecpu(int cpu);

void acquirecpus(cpu_set_t* mask);

void singlemode(void);

void parallelmode(void);

void nodebarrier(void);

void notifymaskchangeto(const cpu_set_t* mask);

void notifymaskchange(void);

void printShmem(void);

#endif //DLB_KERNEL_H
