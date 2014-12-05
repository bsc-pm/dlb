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

#ifndef DLB_INTERFACE_H
#define DLB_INTERFACE_H

#ifdef __cplusplus
extern "C"
{
#endif

void DLB_Init(void);
void DLB_Finalize(void);
void DLB_enable(void);
void DLB_disable(void);
void DLB_single(void);
void DLB_parallel(void);
void DLB_UpdateResources(void);
void DLB_UpdateResources_max(int max_resources);
void DLB_ReturnClaimedCpus(void);
void DLB_Lend(void);
void DLB_Retrieve(void);
int DLB_ReleaseCpu(int cpu);
int DLB_ReturnClaimedCpu(int cpu);
void DLB_ClaimCpus(int cpus);
int DLB_CheckCpuAvailability(int cpu);
int DLB_Is_auto(void);

void DLB_Stats_Init(void);
void DLB_Stats_Finalize(void);
int DLB_Stats_GetNumCpus(void);
double DLB_Stats_GetCpuUsage(int pid);
int DLB_Stats_GetActiveCpus(int pid);
void DLB_Stats_GetLoadAvg(int pid, double *load);

#ifdef __cplusplus
}
#endif

#endif /* DLB_INTERFACE_H */
