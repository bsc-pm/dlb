/*********************************************************************************/
/*  Copyright 2009-2024 Barcelona Supercomputing Center                          */
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
#ifndef LEWI_ASYNC_H
#define LEWI_ASYNC_H

#include "LB_core/spd.h"

int lewi_async_Init(subprocess_descriptor_t *spd);
int lewi_async_Finalize(subprocess_descriptor_t *spd);

int lewi_async_Enable(const subprocess_descriptor_t *spd);
int lewi_async_Disable(const subprocess_descriptor_t *spd);

int lewi_async_IntoBlockingCall(const subprocess_descriptor_t *spd);
int lewi_async_OutOfBlockingCall(const subprocess_descriptor_t *spd);

int lewi_async_Lend(const subprocess_descriptor_t *spd);
int lewi_async_LendCpus(const subprocess_descriptor_t *spd, int ncpus);
int lewi_async_Reclaim(const subprocess_descriptor_t *spd);
int lewi_async_AcquireCpus(const subprocess_descriptor_t *spd, int ncpus);
int lewi_async_Borrow(const subprocess_descriptor_t *spd);
int lewi_async_BorrowCpus(const subprocess_descriptor_t *spd, int ncpus);

#endif /* LEWI_ASYNC_H */
