/*************************************************************************************/
/*      Copyright 2012 Barcelona Supercomputing Center                               */
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

#ifndef LEND_LIGHT_H
#define LEND_LIGHT_H

#define LEND_CPUS 0
#define ACQUIRE_CPUS 1

#include <LB_comm/comm.h>

/******* Main Functions Lend_light Balancing Policy ********/

void Lend_light_Init(int me, int num_procs, int node);

void Lend_light_Finish(void);

void Lend_light_IntoCommunication(void);

void Lend_light_OutOfCommunication(void);

void Lend_light_IntoBlockingCall(int is_iter);

void Lend_light_OutOfBlockingCall(int is_iter);

/******* Auxiliar Functions Lend_light Balancing Policy ********/

int createThreads_Lend_light();

void* masterThread_Lend_light(void* arg);
void applyNewDistribution_light(int* cpus, int except);
int CalculateNewDistribution_Lend_light(int* cpus, int process, ProcMetrics info[]);
int CalculateNewDistribution_Lend_light_siguiente(int* cpus, int process, ProcMetrics info);

void* slaveThread_Lend_light(void* arg);
void Lend_light_updateresources();
void setThreads_Lend_light(int numThreads);

#endif //LEND_H

