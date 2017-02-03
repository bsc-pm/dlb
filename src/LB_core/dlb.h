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

#ifndef DLB_H
#define DLB_H

#include "dlb_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************************/
/*    Status                                                                     */
/*********************************************************************************/

/* \brief Initialize DLB library
 * \param[in] mask
 * \param[in] dlb_args
 * \return
 */
int DLB_Init(const_dlb_cpu_set_t mask, const char *dlb_args);

/* \brief Finalize DLB library
 * \return
 */
int DLB_Finalize(void);

/* \brief Enable DLB
 * \return
 *
 * Only if the library was previously disabled, subsequent DLB calls will be
 * attended again.
 */
int DLB_Enable(void);

/* \brief Disable DLB
 * \return
 *
 * The process will recover the original status of its resources, and further DLB
 * calls will be ignored.
 */
int DLB_Disable(void);

/* \brief DLB will avoid to assign more resources that the maximum provided
 * \param[in] max
 * \return
 */
int DLB_SetMaxParallelism(int max);


/*********************************************************************************/
/*    Callbacks                                                                  */
/*********************************************************************************/

/* \brief Set callback
 * \param[in] which
 * \param[in] callback
 * \return
 */
int DLB_CallbackSet(dlb_callbacks_t which, dlb_callback_t callback);

/* \brief Set callback
 * \param[in] which
 * \param[out] callback
 * \return
 */
int DLB_CallbackGet(dlb_callbacks_t which, dlb_callback_t callback);


/*********************************************************************************/
/*    Lend                                                                       */
/*********************************************************************************/

/* \brief Lend all the current CPUs
 * \return
 */
int DLB_Lend(void);


/* \brief Lend a specific CPU
 * \param[in] cpuid
 * \return
 */
int DLB_LendCpu(int cpuid);

/* \brief
 * \param[in] ncpus
 * \return
 */
int DLB_LendCpus(int ncpus);

/* \brief
 * \param[in] mask
 * \return
 */
int DLB_LendCpuMask(const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Reclaim                                                                    */
/*********************************************************************************/

/* \brief Retrieve the resources owned by the process
 * \return
 *
 * Retrieve the CPUs owned by the process that were previously lent to DLB.
 */
int DLB_Reclaim(void);

/* \brief Retrieve the resources owned by the process
 * \param[in] cpuid
 * \return
 */
int DLB_ReclaimCpu(int cpuid);

/* \brief Claim some of the CPUs owned by this process
 * \param[in] ncpus Number of CPUs to reclaim
 * \return
 *
 * Claim up to 'cpus' CPUs of which this process is the owner
 */
int DLB_ReclaimCpus(int ncpus);

/* \brief
 * \param[in] mask
 * \return
 */
int DLB_ReclaimCpuMask(const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Acquire                                                                    */
/*********************************************************************************/

/* \brief
 * \return
 */
int DLB_Acquire(void);

/* \brief Try to acquire a specific CPU
 * \param[in] cpu CPU to acquire
 * \return
 *
 * Whenever is possible, DLB will assign the specified CPU to the current process
 */
int DLB_AcquireCpu(int cpuid);

/* \brief
 * \param[in] ncpus
 * \return
 */
int DLB_AcquireCpus(int ncpus);

/* \brief Try to acquire a set of CPUs
 * \param[in] mask CPU set to acquire
 * \return
 *
 * Whenever is possible, DLB will assign the specified CPUs to the current process
 */
int DLB_AcquireCpuMask(const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Return                                                                     */
/*********************************************************************************/

/* \brief Return claimed CPUs of other processes
 * \return
 *
 * Return those external CPUs that are currently assigned to me and have been claimed
 * by their owner.
 */
int DLB_Return(void);

/* \brief Return the specific CPU if it has been reclaimed
 * \param[in] cpuid CPU to return
 * \return
 *
 * Return the specific CPU if it is currently assigned to me and has been claimed
 * by its owner.
 */
int DLB_ReturnCpu(int cpuid);


/*********************************************************************************/
/*    DROM Responsive                                                            */
/*********************************************************************************/

/* \brief
 * \param[out] nthreads
 * \param[out] mask
 * \return
 */
int DLB_PollDROM(int *nthreads, dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Misc                                                                       */
/*********************************************************************************/

/* \brief Check current status of a CPU
 * \param[in] cpuid CPU to be checked
 * \return
 *
 * Check whether the specified CPU is being used by another process
 */
int DLB_CheckCpuAvailability(int cpuid);

/* \brief Barrier between processes in the node
 * \return
 */
int DLB_Barrier(void);

/* \brief Set DLB internal variable
 * \param[in] variable Internal variable to set
 * \param[in] value New value
 * \return
 *
 * Change the value of a DLB internal variable
 */
int DLB_SetVariable(const char *variable, const char *value);

/* \brief Get DLB internal variable
 * \param[in] variable Internal variable to set
 * \param[out] value Current DLB variable value
 * \return
 *
 * Query the value of a DLB internal variable
 */
int DLB_GetVariable(const char *variable, char *value);

/* \brief Print DLB internal variables
 * \return
 */
int DLB_PrintVariables(void);

/* \brief Print the data stored in the shmem
 * \return
 */
int DLB_PrintShmem(void);

/* \brief
 * \param[in] errnum
 * \return
 */
const char* DLB_Strerror(int errnum);


#ifdef __cplusplus
}
#endif

#endif /* DLB_H */
