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

#ifndef LEND_SIMPLE_H
#define LEND_SIMPLE_H

#define LEND_CPUS 0
#define ACQUIRE_CPUS 1

#include <LB_comm/comm.h>

/******* Main Functions Lend Balancing Policy ********/

void Lend_simple_Init(int me, int num_procs, int node);

void Lend_simple_Finish(void);

void Lend_simple_InitIteration();

void Lend_simple_FinishIteration(double cpuSecs, double MPISecs);

void Lend_simple_IntoCommunication(void);

void Lend_simple_OutOfCommunication(void);

void Lend_simple_IntoBlockingCall(double cpuSecs, double MPISecs);

void Lend_simple_OutOfBlockingCall(void);

/******* Auxiliar Functions Lend Balancing Policy ********/

void createThreads_Lend_simple();

void* masterThread_Lend_simple(void* arg);
void applyNewDistribution_Lend_simple(int* cpus, int except);
int CalculateNewDistribution_Lend_simple(int* cpus, int process, ProcMetrics info);

void* slaveThread_Lend_simple(void* arg);
void Lend_simple_updateresources();

#endif //LEND_SIMPLE_H

