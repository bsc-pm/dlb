/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

#ifndef LEWI_MASK_H
#define LEWI_MASK_H

#include "LB_core/spd.h"

int lewi_mask_Init(subprocess_descriptor_t *spd);
int lewi_mask_Finalize(subprocess_descriptor_t *spd);
int lewi_mask_EnableDLB(const subprocess_descriptor_t *spd);
int lewi_mask_DisableDLB(const subprocess_descriptor_t *spd);
int lewi_mask_SetMaxParallelism(const subprocess_descriptor_t *spd, int max);
int lewi_mask_UnsetMaxParallelism(const subprocess_descriptor_t *spd);

int lewi_mask_IntoBlockingCall(const subprocess_descriptor_t *spd);
int lewi_mask_OutOfBlockingCall(const subprocess_descriptor_t *spd);

int lewi_mask_Lend(const subprocess_descriptor_t *spd);
int lewi_mask_LendCpu(const subprocess_descriptor_t *spd, int cpuid);
int lewi_mask_LendCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

int lewi_mask_Reclaim(const subprocess_descriptor_t *spd);
int lewi_mask_ReclaimCpu(const subprocess_descriptor_t *spd, int cpuid);
int lewi_mask_ReclaimCpus(const subprocess_descriptor_t *spd, int ncpus);
int lewi_mask_ReclaimCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

int lewi_mask_AcquireCpu(const subprocess_descriptor_t *spd, int cpuid);
int lewi_mask_AcquireCpus(const subprocess_descriptor_t *spd, int ncpus);
int lewi_mask_AcquireCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);
int lewi_mask_AcquireCpusInMask(const subprocess_descriptor_t *spd, int ncpus, const cpu_set_t *mask);

int lewi_mask_Borrow(const subprocess_descriptor_t *spd);
int lewi_mask_BorrowCpu(const subprocess_descriptor_t *spd, int cpuid);
int lewi_mask_BorrowCpus(const subprocess_descriptor_t *spd, int ncpus);
int lewi_mask_BorrowCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);
int lewi_mask_BorrowCpusInMask(const subprocess_descriptor_t *spd, int ncpus, const cpu_set_t *mask);

int lewi_mask_Return(const subprocess_descriptor_t *spd);
int lewi_mask_ReturnCpu(const subprocess_descriptor_t *spd, int cpuid);
int lewi_mask_ReturnCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

int lewi_mask_CheckCpuAvailability(const subprocess_descriptor_t *spd, int cpuid);
int lewi_mask_UpdateOwnership(const subprocess_descriptor_t *spd, const cpu_set_t *process_mask);

#endif /* LEWI_MASK_H */
