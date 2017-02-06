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

#ifndef DWB_Eco_H
#define DWB_Eco_H

#include <LB_comm/comm.h>

/******* Main Functions DWB_Eco Balancing Policy ********/

void DWB_Eco_Init(int meId, int num_procs, int nodeId);

void DWB_Eco_Finish(void);

void DWB_Eco_InitIteration(void);

void DWB_Eco_FinishIteration(double cpuSecs, double MPISecs);

void DWB_Eco_IntoCommunication(void);

void DWB_Eco_OutOfCommunication(void);

void DWB_Eco_IntoBlockingCall(double cpuSecs, double MPISecs);

void DWB_Eco_OutOfBlockingCall(void);

void DWB_Eco_updateresources();

/******* Auxiliar Functions DWB_Eco Balancing Policy ********/

void* masterThread_DWB_Eco(void* arg);
void* slaveThread_DWB_Eco(void* arg);

void GetMetrics_DWB_Eco(ProcMetrics metrics[]);
//void getLocaMetrics(ProcMetrics *metr);
void SendLocalMetrics_DWB_Eco(ProcMetrics LM);
void CalculateNewDistribution_DWB_Eco(ProcMetrics LM[], int cpus[]);
void applyNewDistribution_DWB_Eco(int cpus[]);

#endif //DWB_Eco_H

