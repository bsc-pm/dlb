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

#ifndef AUTO_LEWI_MASK_H
#define AUTO_LEWI_MASK_H

#include "LB_core/spd.h"
#include <sched.h>

/******* Main Functions - LeWI Mask Balancing Policy ********/

int auto_lewi_mask_Init(const subprocess_descriptor_t *spd);
int auto_lewi_mask_Finish(const subprocess_descriptor_t *spd);
int auto_lewi_mask_EnableDLB(const subprocess_descriptor_t *spd);
int auto_lewi_mask_DisableDLB(const subprocess_descriptor_t *spd);

void auto_lewi_mask_IntoCommunication(const subprocess_descriptor_t *spd);
void auto_lewi_mask_OutOfCommunication(const subprocess_descriptor_t *spd);
void auto_lewi_mask_IntoBlockingCall(const subprocess_descriptor_t *spd);
void auto_lewi_mask_OutOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter);

int auto_lewi_mask_Lend(const subprocess_descriptor_t *spd);
int auto_lewi_mask_LendCpu(const subprocess_descriptor_t *spd, int cpuid);

int auto_lewi_mask_ReclaimCpus(const subprocess_descriptor_t *spd, int ncpus);

int auto_lewi_mask_AcquireCpu(const subprocess_descriptor_t *spd, int cpuid);
int auto_lewi_mask_AcquireCpuMask(const subprocess_descriptor_t *spd, const cpu_set_t *mask);

int auto_lewi_mask_BorrowCpus(const subprocess_descriptor_t *spd, int ncpus);

int auto_lewi_mask_ReturnCpu(const subprocess_descriptor_t *spd, int cpuid);

int auto_lewi_mask_CheckCpuAvailability(const subprocess_descriptor_t *spd, int cpuid);



#endif /* AUTO_LEWI_MASK_H */

