/*********************************************************************************/
/*  Copyright 2017 Barcelona Supercomputing Center                               */
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

#ifndef NEW_H
#define NEW_H

#include "LB_core/spd.h"

int new_Init(const subprocess_descriptor_t *spd);
int new_Finish(const subprocess_descriptor_t *spd);

int new_Lend(const subprocess_descriptor_t *spd);
int new_LendCpu(const subprocess_descriptor_t *spd, int cpuid);
int new_LendCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

int new_Reclaim(const subprocess_descriptor_t *spd);
int new_ReclaimCpu(const subprocess_descriptor_t *spd, int cpuid);
int new_ReclaimCpus(const subprocess_descriptor_t *spd, int ncpus);
int new_ReclaimCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

int new_Acquire(const subprocess_descriptor_t *spd);
int new_AcquireCpu(const subprocess_descriptor_t *spd, int cpuid);
int new_AcquireCpus(const subprocess_descriptor_t *spd, int ncpus);
int new_AcquireCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

int new_Borrow(const subprocess_descriptor_t *spd);
int new_BorrowCpu(const subprocess_descriptor_t *spd, int cpuid);
int new_BorrowCpus(const subprocess_descriptor_t *spd, int ncpus);
int new_BorrowCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

int new_Return(const subprocess_descriptor_t *spd);
int new_ReturnCpu(const subprocess_descriptor_t *spd, int cpuid);
int new_ReturnCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

int new_CheckCpuAvailability(const subprocess_descriptor_t *spd, int cpuid);

#endif /* NEW_H */
