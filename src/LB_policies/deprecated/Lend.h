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

#ifndef LEND_H
#define LEND_H

#define LEND_CPUS 0
#define ACQUIRE_CPUS 1

#include <LB_comm/comm.h>

/******* Main Functions Lend Balancing Policy ********/

void Lend_Init(int me, int num_procs, int node);

void Lend_Finish(void);

void Lend_InitIteration();

void Lend_FinishIteration(double cpuSecs, double MPISecs);

void Lend_IntoCommunication(void);

void Lend_OutOfCommunication(void);

void Lend_IntoBlockingCall(double cpuSecs, double MPISecs);

void Lend_OutOfBlockingCall(void);

/******* Auxiliar Functions Lend Balancing Policy ********/

void createThreads_Lend();

void* masterThread_Lend(void* arg);
void applyNewDistribution(int* cpus, int except);
int CalculateNewDistribution_Lend(int* cpus, int process, ProcMetrics info[]);
int CalculateNewDistribution_Lend_siguiente(int* cpus, int process, ProcMetrics info);

void* slaveThread_Lend(void* arg);
void Lend_updateresources();
void setThreads_Lend(int numThreads);
void assign_lended_cpus(int* cpus, int process, ProcMetrics info[]);
void retrieve_cpus(int* cpus, int process, ProcMetrics info[]);

#endif //LEND_H

