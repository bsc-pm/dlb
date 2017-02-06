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

#ifndef LEND_MAP_H
#define LEND_MAP_H

#define LEND_CPUS 0
#define ACQUIRE_CPUS 1

#include <sched.h>

/******* Main Functions Map Balancing Policy ********/

void Map_Init(const cpu_set_t *process_mask);

void Map_Finish(void);

void Map_IntoCommunication(void);

void Map_OutOfCommunication(void);

void Map_IntoBlockingCall(int is_iter, int blocking_mode);

void Map_OutOfBlockingCall(int is_iter);

void Map_updateresources(int max_cpus);

/******* Auxiliar Functions Map Balancing Policy ********/

void setThreads_Map(int numThreads, int action, int* cpus);

#endif //LEND_H

