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

#include <LB_comm/comm.h>

/******* Main Functions Lend_light Balancing Policy ********/

void Lend_light_Init(void);

void Lend_light_Finish(void);

void Lend_light_IntoCommunication(void);

void Lend_light_OutOfCommunication(void);

void Lend_light_IntoBlockingCall(int is_iter, int blocking_mode);

void Lend_light_OutOfBlockingCall(int is_iter);

void Lend_light_updateresources(int maxResources);

void Lend_light_resetDLB(void);

void Lend_light_disableDLB(void);

void Lend_light_enableDLB(void);

void Lend_light_single(void);

void Lend_light_parallel(void);

/******* Auxiliar Functions Lend_light Balancing Policy ********/

void setThreads_Lend_light(int numThreads);

#endif //LEND_H

