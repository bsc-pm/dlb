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

#ifndef WEIGHT_H
#define WEIGHT_H


#include <LB_comm/comm.h>
/******* Main Functions Weight Balancing Policy ********/

void Weight_Init(void);

void Weight_Finish(void);

void Weight_IntoCommunication(void);

void Weight_OutOfCommunication(void);

void Weight_IntoBlockingCall(int is_iter, int blocking_mode);

void Weight_OutOfBlockingCall(int is_iter);

void Weight_updateresources();

/******* Auxiliar Functions Weight Balancing Policy ********/

void* masterThread_Weight(void* arg);
void* slaveThread_Weight(void* arg);

void createThreads_Weight();

void GetMetrics(ProcMetrics metrics[]);
void getLocaMetrics(ProcMetrics *metr);
void SendLocalMetrics(ProcMetrics LM);
void CalculateNewDistribution_Weight(ProcMetrics LM[], int cpus[]);
void applyNewDistribution_Weight(int cpus[]);

#endif //WEIGHT_H

