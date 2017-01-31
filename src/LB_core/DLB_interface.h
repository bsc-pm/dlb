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

#ifndef DLB_INTERFACE_H
#define DLB_INTERFACE_H

#include "LB_core/dlb_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/* \brief Initialize DLB library
 */
int DLB_Init(const_dlb_cpu_set_t mask, const char *lb_args);

/* \brief Finalize DLB library
 */
int DLB_Finalize(void);

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

/* \brief Try to acquire a set of CPUs
 * \param[in] mask CPU set to acquire
 *
 * Whenever is possible, DLB will assign the specified CPUs to the current process
 */
void DLB_AcquireCpus(dlb_cpu_set_t mask);

/* \brief Check current status of a CPU
 * \param[in] cpu CPU to be checked
 * \return True if the CPU is not being used by anyone else
 *
 * Check whether the specified CPU is being used by another process
 */
int DLB_CheckCpuAvailability(int cpu);

/* \brief Update DLB status
 *
 * Update the status of 'Statistics' and 'DROM' modules, like updating the process
 * statistics or check if some other process has signaled a new process mask.
 */
void DLB_Update(void);

// FIXME API in testing
int DLB_poll_new_threads(int *nthreads, dlb_cpu_set_t mask);

/* \brief Barrier between processes in the node
 */
void DLB_Barrier(void);

/* \brief Notify DLB that the Process Mask has changed
 *
 * Notify DLB that the process affinity mask has been changed. DLB will then query
 * the runtime to obtain the current mask.
 */
void DLB_NotifyProcessMaskChange(void);

/* \brief Notify DLB that the Process Mask has changed
 * \param[in] mask The current process_mask
 *
 * Notify DLB that the process affinity mask has been changed
 */
void DLB_NotifyProcessMaskChangeTo(const_dlb_cpu_set_t mask);

/* \brief Print the data stored in the shmem
 */
void DLB_PrintShmem(void);

/* \brief Set DLB internal variable
 * \param[in] variable Internal variable to set
 * \param[in] value New value
 * \return 0 on success, -1 otherwise
 *
 * Change the value of a DLB internal variable
 */
int DLB_SetVariable(const char *variable, const char *value);

/* \brief Get DLB internal variable
 * \param[in] variable Internal variable to set
 * \param[out] value Current DLB variable value
 * \return 0 on success, -1 otherwise
 *
 * Query the value of a DLB internal variable
 */
int DLB_GetVariable(const char *variable, char *value);

/* \brief Print DLB internal variables
 */
void DLB_PrintVariables(void);

const char* DLB_strerror(int errnum);


/*******************************************************/
/*    MPI Interface                                    */
/*******************************************************/

/* \brief Blocks until all processes in the same node have reached this routine.
 * */
void DLB_MPI_node_barrier(void);

#ifdef __cplusplus
}
#endif

#endif /* DLB_INTERFACE_H */
