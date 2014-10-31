/*************************************************************************************/
/*      Copyright 2014 Barcelona Supercomputing Center                               */
/*                                                                                   */
/*      This file is part of the DLB library.                                        */
/*                                                                                   */
/*      DLB is free software: you can redistribute it and/or modify                  */
/*      it under the terms of the GNU Lesser General Public License as published by  */
/*      the Free Software Foundation, either version 3 of the License, or            */
/*      (at your option) any later version.                                          */
/*                                                                                   */
/*      DLB is distributed in the hope that it will be useful,                       */
/*      but WITHOUT ANY WARRANTY; without even the implied warranty of               */
/*      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                */
/*      GNU Lesser General Public License for more details.                          */
/*                                                                                   */
/*      You should have received a copy of the GNU Lesser General Public License     */
/*      along with DLB.  If not, see <http://www.gnu.org/licenses/>.                 */
/*************************************************************************************/

#ifndef LEWI_A_H
#define LEWI_A_H

#define LEND_CPUS 0
#define ACQUIRE_CPUS 1
#define ASK_4_CPUS 2

#include <LB_comm/comm.h>

/******* Main Functions LeWI_A Balancing Policy ********/

void LeWI_A_Init(int me, int num_procs, int node);

void LeWI_A_Finish(void);

void LeWI_A_InitIteration();

void LeWI_A_FinishIteration(double cpuSecs, double MPISecs);

void LeWI_A_IntoCommunication(void);

void LeWI_A_OutOfCommunication(void);

void LeWI_A_IntoBlockingCall(double cpuSecs, double MPISecs);

void LeWI_A_OutOfBlockingCall(void);

void LeWI_A_retrieve_cpus(int* cpus, int process, ProcMetrics info[]);
/******* Auxiliar Functions LeWI_A Balancing Policy ********/

void createThreads_LeWI_A();

void* masterThread_LeWI_A(void* arg);
void LeWI_A_applyNewDistribution(int* cpus, int except);

void* slaveThread_LeWI_A(void* arg);
void LeWI_A_updateresources();
void setThreads_LeWI_A(int numThreads);

#endif //LEWI_A_H

