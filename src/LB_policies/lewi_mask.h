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

#ifndef LEWI_MASK_H
#define LEWI_MASK_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <sched.h>

#include "LB_core/spd.h"

/******* Main Functions - LeWI Mask Balancing Policy ********/

void lewi_mask_Init(const subprocess_descriptor_t *spd);
void lewi_mask_Finish(const subprocess_descriptor_t *spd);
void lewi_mask_enableDLB(const subprocess_descriptor_t *spd);
void lewi_mask_disableDLB(const subprocess_descriptor_t *spd);

void lewi_mask_IntoCommunication(const subprocess_descriptor_t *spd);
void lewi_mask_OutOfCommunication(const subprocess_descriptor_t *spd);
void lewi_mask_IntoBlockingCall(const subprocess_descriptor_t *spd);
void lewi_mask_OutOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter);

void lewi_mask_UpdateResources(const subprocess_descriptor_t *spd, int max_resources);
void lewi_mask_ReturnClaimedCpus(const subprocess_descriptor_t *spd);
void lewi_mask_ClaimCpus(const subprocess_descriptor_t *spd, int cpus);
void lewi_mask_acquireCpu(const subprocess_descriptor_t *spd, int cpu);
void lewi_mask_acquireCpus(const subprocess_descriptor_t *spd, const cpu_set_t* cpus);




#endif /* LEWI_MASK_H */

