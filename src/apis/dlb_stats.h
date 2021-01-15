/*********************************************************************************/
/*  Copyright 2009-2021 Barcelona Supercomputing Center                          */
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
/*  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                */
/*********************************************************************************/

#ifndef DLB_STATS_H
#define DLB_STATS_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************************/
/*    Statistics Module                                                          */
/*********************************************************************************/

/*! \brief Initialize DLB Statistics Module
 *  \return error code
 */
int DLB_Stats_Init(void);

/*! \brief Finalize DLB Statistics Module
 *  \return error code
 */
int DLB_Stats_Finalize(void);

/*! \brief Get the total number of available CPUs in the node
 *  \param[out] ncpus the number of CPUs
 *  \return error code
 */
int DLB_Stats_GetNumCpus(int *ncpus);

/*! \brief Get the PID's attached to this module
 *  \param[out] pidlist The output list
 *  \param[out] nelems Number of elements in the list
 *  \param[in] max_len Max capacity of the list
 *  \return error code
 */
int DLB_Stats_GetPidList(int *pidlist,int *nelems,int max_len);

/*! \brief Get the CPU Usage of the given PID
 *  \param[in] pid PID to consult
 *  \param[out] usage the CPU usage of the given PID, or -1.0 if not found
 *  \return error code
 */
int DLB_Stats_GetCpuUsage(int pid, double *usage);

/*! \brief Get the CPU Average Usage of the given PID
 *  \param[in] pid PID to consult
 *  \param[out] usage the CPU usage of the given PID, or -1.0 if not found
 *  \return error code
 */
int DLB_Stats_GetCpuAvgUsage(int pid, double *usage);

/*! \brief Get the CPU usage of all the attached PIDs
 *  \param[out] usagelist The output list
 *  \param[out] nelems Number of elements in the list
 *  \param[in] max_len Max capacity of the list
 *  \return error code
 */
int DLB_Stats_GetCpuUsageList(double *usagelist,int *nelems,int max_len);

/*! \brief Get the CPU Average usage of all the attached PIDs
 *  \param[out] avgusagelist The output list
 *  \param[out] nelems Number of elements in the list
 *  \param[in] max_len Max capacity of the list
 *  \return error code
 */
int DLB_Stats_GetCpuAvgUsageList(double *avgusagelist,int *nelems,int max_len);

/*! \brief Get the CPU Usage of all the DLB processes in the node
 *  \param[out] usage the Node Usage
 *  \return error code
 */
int DLB_Stats_GetNodeUsage(double *usage);

/*! \brief Get the CPU Average Usage of all the DLB processes in the node
 *  \param[out] usage the Node Usage
 *  \return error code
 */
int DLB_Stats_GetNodeAvgUsage(double *usage);

/*! \brief Get the number of CPUs assigned to a given process
 *  \param[in] pid Process ID to consult
 *  \param[out] ncpus the number of CPUs used by a process
 *  \return error code
 */
int DLB_Stats_GetActiveCpus(int pid, int *ncpus);

/*! \brief Get the number of CPUs assigned to each process
 *  \param[out] cpuslist The output list
 *  \param[out] nelems Number of elements in the list
 *  \param[in] max_len Max capacity of the list
 *  \return error code
 */
int DLB_Stats_GetActiveCpusList(int *cpuslist,int *nelems,int max_len);

/*! \brief Get the Load Average of a given process
 *  \param[in] pid Process ID to consult
 *  \param[out] load double[3] Load Average ( 1min 5min 15min )
 *  \return error code
 */
int DLB_Stats_GetLoadAvg(int pid, double *load);

/*! \brief Get the percentage of time that the CPU has been in state IDLE
 *  \param[in] cpu CPU id
 *  \param[out] percentage percentage of state/total
 *  \return error code
 */
int DLB_Stats_GetCpuStateIdle(int cpu, float *percentage);

/*! \brief Get the percentage of time that the CPU has been in state OWNED
 *  \param[in] cpu CPU id
 *  \param[out] percentage percentage of state/total
 *  \return error code
 */
int DLB_Stats_GetCpuStateOwned(int cpu, float *percentage);

/*! \brief Get the percentage of time that the CPU has been in state GUESTED
 *  \param[in] cpu CPU id
 *  \param[out] percentage percentage of state/total
 *  \return error code
 */
int DLB_Stats_GetCpuStateGuested(int cpu, float *percentage);

#ifdef __cplusplus
}
#endif

#endif /* DLB_STATS_H */
