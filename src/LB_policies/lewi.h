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

#ifndef LEWI_H
#define LEWI_H

#include "LB_core/spd.h"

int lewi_Init(const subprocess_descriptor_t *spd);
int lewi_Finish(const subprocess_descriptor_t *spd);
int lewi_EnableDLB(const subprocess_descriptor_t *spd);
int lewi_DisableDLB(const subprocess_descriptor_t *spd);

int lewi_IntoCommunication(const subprocess_descriptor_t *spd);
int lewi_OutOfCommunication(const subprocess_descriptor_t *spd);
int lewi_IntoBlockingCall(const subprocess_descriptor_t *spd);
int lewi_OutOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter);

int lewi_Lend(const subprocess_descriptor_t *spd);

int lewi_Reclaim(const subprocess_descriptor_t *spd);

int lewi_Borrow(const subprocess_descriptor_t *spd);
int lewi_BorrowCpus(const subprocess_descriptor_t *spd, int ncpus);

#endif /* LEWI_H */
