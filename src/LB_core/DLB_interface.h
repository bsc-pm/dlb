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

/* DLB custom types */
typedef void* dlb_cpu_set_t;

#ifdef __cplusplus
extern "C"
{
#endif

/* \brief Initialize DLB library
 */
void DLB_Init(void);

/* \brief Finalize DLB library
 */
void DLB_Finalize(void);

/* \brief Disable DLB
 *
 * The process will recover the original status of its resources, and further DLB
 * calls will be ignored.
 */
void DLB_disable(void);

/* \brief Enable DLB
 *
 * Only if the library was previously disabled, subsequent DLB calls will be
 * attended again.
 */
void DLB_enable(void);

/* \brief Reset the process resources
 *
 * Recover the default CPUs and return the others.
 */
void DLB_reset(void);

/* \brief Inform to DLB of a serial code incoming
 *
 * DLB will leave the process with only 1 CPU running, other CPUs might be lent
 */
void DLB_single(void);

/* \brief Inform to DLB of a parallel code incoming
 *
 * DLB will reclaim the original CPUs of the process. Useful after a DLB_reset.
 */
void DLB_parallel(void);

/* \brief Ask for more resources
 *
 * Query DLB for available resources to use. If other processes have lent some CPU,
 * DLB will assign those resources to the current process.
 */
void DLB_UpdateResources(void);

/* \brief Ask for more resources
 * \param[in] max_resources Maximum number of CPUs that the process wants to borrow
 *
 * Query DLB for available resources to use. If other processes have lent some CPU,
 * DLB will assign those resources to the current process.
 */
void DLB_UpdateResources_max(int max_resources);

/* \brief Return claimed CPUs of other processes
 *
 * Return those external CPUs that are currently assigned to me and have been claimed
 * by their owner.
 */
void DLB_ReturnClaimedCpus(void);

/* \brief Lend all the current CPUs
 *
 * Lend the current CPUs assigned to this process and let DLB handle them. The current
 * process will continue running with 1 CPU.
 */
void DLB_Lend(void);

/* \brief Retrieve the resources owned by the process
 *
 * Retrieve the CPUs owned by the process that were previously lent to DLB.
 */
void DLB_Retrieve(void);

/* \brief Lend a specific CPU
 * \param[in] cpu CPU to lend
 * \return True on success
 *
 * Release the specified CPU from the process and lend it to DLB
 */
int DLB_ReleaseCpu(int cpu);

/* \brief Return the specific CPU if it has been reclaimed
 * \param[in] cpu CPU to return
 * \return True if the CPU was reclaimed, and therefore returned
 *
 * Return the specific CPU if it is currently assigned to me and has been claimed
 * by its owner.
 */
int DLB_ReturnClaimedCpu(int cpu);

/* \brief Claim some of the CPUs owned by this process
 * \param[in] cpus Number of CPUs to claim
 *
 * Claim up to 'cpus' CPUs of which this process is the owner
 */
void DLB_ClaimCpus(int cpus);

/* \brief Try to acquire a specific CPU
 * \param[in] cpu CPU to acquire
 *
 * Whenever is possible, DLB will assign the specified CPU to the current process
 */
void DLB_AcquireCpu(int cpu);

/* \brief Check current status of a CPU
 * \param[in] cpu CPU to be checked
 * \return True if the CPU is not being used by anyone else
 *
 * Check whether the specified CPU is being used by another process
 */
int DLB_CheckCpuAvailability(int cpu);

/* \brief Checks if the current policy is auto_LeWI_mask
 * \return True if the condition is met
 */
int DLB_Is_auto(void);

/* \brief Update DLB status
 *
 * Update the status of 'Statistics' and 'DROM' modules, like updating the process
 * statistics or check if some other process has signaled a new process mask.
 */
void DLB_Update(void);

/* \brief Print the data stored in the shmem
 */
void DLB_PrintShmem(void);

/*******************************************************/
/*    Statistics Module                                */
/*******************************************************/

/* \brief Initialize DLB Statistics Module
 */
void DLB_Stats_Init(void);

/* \brief Finalize DLB Statistics Module
 */
void DLB_Stats_Finalize(void);

/* \brief Get the total number of available CPUs in the node
 * \return the number of CPUs
 */
int DLB_Stats_GetNumCpus(void);

/* \brief Get the PID's attached to this module
 * \param[out] pidlist The output list
 * \param[out] nelems Number of elements in the list
 * \param[in] max_len Max capacity of the list
 */
void DLB_Stats_GetPidList(int *pidlist,int *nelems,int max_len);

/* \brief Get the CPU Usage of the given PID
 * \param[in] pid PID to consult
 * \return the CPU usage of the given PID, or -1.0 if not found
 */
double DLB_Stats_GetCpuUsage(int pid);

/* \brief Get the CPU usage of all the attached PIDs
 * \param[out] usagelist The output list
 * \param[out] nelems Number of elements in the list
 * \param[in] max_len Max capacity of the list
 */
void DLB_Stats_GetCpuUsageList(double *usagelist,int *nelems,int max_len);

/* \brief Get the number of CPUs assigned to a given process
 * \param[in] pid Process ID to consult
 * \return the number of CPUs used by a process, or -1 if not found
 */
int DLB_Stats_GetActiveCpus(int pid);

/* \brief Get the number of CPUs assigned to each process
 * \param[out] cpuslist The output list
 * \param[out] nelems Number of elements in the list
 * \param[in] max_len Max capacity of the list
 */
void DLB_Stats_GetActiveCpusList(int *cpuslist,int *nelems,int max_len);

/* \brief Get the Load Average of a given process
 * \param[in] pid Process ID to consult
 * \param[out] load[3] Load Average ( 1min 5min 15min )
 * \return 0 success, -1 if pid not found
 */
int DLB_Stats_GetLoadAvg(int pid, double *load);

/* \brief Print the data stored in the Stats shmem
 */
void DLB_Stats_PrintShmem(void);

/*******************************************************/
/*    Dynamic Resource Manager Module                  */
/*******************************************************/

/* \brief Initialize DROM Module
 */
void DLB_Drom_Init(void);

/* \brief Finalize DROM Module
 */
void DLB_Drom_Finalize(void);

/* \brief Get the total number of available CPUs in the node
 * \return the number of CPUs
 */
int DLB_Drom_GetNumCpus(void);

/* \brief Get the PID's attached to this module
 * \param[out] pidlist The output list
 * \param[out] nelems Number of elements in the list
 * \param[in] max_len Max capacity of the list
 */
void DLB_Drom_GetPidList(int *pidlist, int *nelems, int max_len);

/* \brief Get the process mask of the given PID
 * \param[in] pid Process ID to consult
 * \param[out] mask Current process mask
 * \return 0 on success, -1 if pid not found
 */
int DLB_Drom_GetProcessMask(int pid, dlb_cpu_set_t mask);

/* \brief Set the process mask of the given PID
 * \param[in] pid Process ID to signal
 * \param[in] mask Process mask to set
 * \return 0 on success, -1 if pid not found
 */
int DLB_Drom_SetProcessMask(int pid, const dlb_cpu_set_t mask);

/* \brief Print the data stored in the Drom shmem
 */
void DLB_Drom_PrintShmem(void);

#ifdef __cplusplus
}
#endif

#endif /* DLB_INTERFACE_H */
