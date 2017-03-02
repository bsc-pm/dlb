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
int new_LendCpu(const subprocess_descriptor_t *spd, int cpuid);
int new_Reclaim(const subprocess_descriptor_t *spd);
int new_ReclaimCpu(const subprocess_descriptor_t *spd, int cpuid);
int new_AcquireCpu(const subprocess_descriptor_t *spd, int cpuid);
int new_ReturnCpu(const subprocess_descriptor_t *spd, int cpuid);

#endif /* NEW_H */
