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

#ifndef LEND_LIGHT_H
#define LEND_LIGHT_H

#define LEND_CPUS 0
#define ACQUIRE_CPUS 1

#include "LB_core/spd.h"

/******* Main Functions Lend_light Balancing Policy ********/

int Lend_light_Init(const subprocess_descriptor_t *spd);
int Lend_light_Finish(const subprocess_descriptor_t *spd);
int Lend_light_enableDLB(const subprocess_descriptor_t *spd);
int Lend_light_disableDLB(const subprocess_descriptor_t *spd);

void Lend_light_IntoCommunication(const subprocess_descriptor_t *spd);
void Lend_light_OutOfCommunication(const subprocess_descriptor_t *spd);
void Lend_light_IntoBlockingCall(const subprocess_descriptor_t *spd);
void Lend_light_OutOfBlockingCall(const subprocess_descriptor_t *spd, int is_iter);

int Lend_light_updateresources(const subprocess_descriptor_t *spd, int maxResources);

#endif /* LEND_LIGHT_H */
