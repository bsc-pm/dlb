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

#ifndef DLB_H
#define DLB_H

#include "dlb_types.h"
#include "dlb_errors.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************************/
/*    Status                                                                     */
/*********************************************************************************/

/*! \brief Initialize DLB library
 *  \param[in] ncpus optional, initial number of CPUs
 *  \param[in] mask optional, initial CPU mask to register
 *  \param[in] dlb_args optional parameter to overwrite LB_ARGS options, or NULL otherwise
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_INIT if DLB is already initialized
 */
int DLB_Init(int ncpus, const_dlb_cpu_set_t mask, const char *dlb_args);

/*! \brief Finalize DLB library
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 */
int DLB_Finalize(void);

/*! \brief Enable DLB
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *
 *  Only if the library was previously disabled, subsequent DLB calls will be
 *  attended again.
 */
int DLB_Enable(void);

/*! \brief Disable DLB
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *
 *  The process will recover the original status of its resources, and further DLB
 *  calls will be ignored.
 */
int DLB_Disable(void);

/*! \brief Set a maximum number of resources to be acquired by the calling process
 *  \param[in] max max number of CPUs
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 */
int DLB_SetMaxParallelism(int max);


/*********************************************************************************/
/*    Callbacks                                                                  */
/*********************************************************************************/

/*! \brief Set callback
 *  \param[in] which callback type
 *  \param[in] callback function pointer to register
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOCBK if the callback type does not exist
 */
int DLB_CallbackSet(dlb_callbacks_t which, dlb_callback_t callback);

/*! \brief Get callback
 *  \param[in] which callback type
 *  \param[out] callback registered callback function for the specified callback type
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOCBK if the callback type does not exist
 */
int DLB_CallbackGet(dlb_callbacks_t which, dlb_callback_t *callback);


/*********************************************************************************/
/*    Lend                                                                       */
/*********************************************************************************/

/*! \brief Lend all the current CPUs
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be lent
 */
int DLB_Lend(void);

/*! \brief Lend a specific CPU
 *  \param[in] cpuid CPU id to lend
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be lent
 */
int DLB_LendCpu(int cpuid);

/*! \brief Lend a specific amount of CPUs, only useful for systems that do not
 *          work with cpu masks
 *  \param[in] ncpus number of CPUs to lend
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be lent
 */
int DLB_LendCpus(int ncpus);

/*! \brief Lend a set of CPUs
 *  \param[in] mask CPU mask to lend
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be lent
 */
int DLB_LendCpuMask(const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Reclaim                                                                    */
/*********************************************************************************/

/*! \brief Reclaim all the CPUs that are owned by the process
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be reclaimed
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 */
int DLB_Reclaim(void);

/*! \brief Reclaim a specific CPU that are owned by the process
 *  \param[in] cpuid CPU id to reclaim
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be reclaimed
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 */
int DLB_ReclaimCpu(int cpuid);

/*! \brief Reclaim a specific amount of CPUs that are owned by the process
 *  \param[in] ncpus Number of CPUs to reclaim
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be reclaimed
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 */
int DLB_ReclaimCpus(int ncpus);

/*! \brief Reclaim a set of CPUs
 *  \param[in] mask CPU mask to reclaim
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be reclaimed
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 */
int DLB_ReclaimCpuMask(const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Acquire                                                                    */
/*********************************************************************************/

/*! \brief Acquire all the possible CPUs registered on DLB
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be acquired
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 *  \return DLB_ERR_REQST if there are too many requests for these resources
 */
int DLB_Acquire(void);

/*! \brief Acquire a specific CPU
 *  \param[in] cpuid cpu CPU to acquire
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be acquired
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 *  \return DLB_ERR_REQST if there are too many requests for this resource
 */
int DLB_AcquireCpu(int cpuid);

/*! \brief Acquire a specific amount of CPUs
 *  \param[in] ncpus Number of CPUs to acquire
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be acquired
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 *  \return DLB_ERR_REQST if there are too many requests for these resources
 */
int DLB_AcquireCpus(int ncpus);

/*! \brief Acquire a set of CPUs
 *  \param[in] mask CPU set to acquire
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be acquired
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 *  \return DLB_ERR_REQST if there are too many requests for these resources
 */
int DLB_AcquireCpuMask(const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Return                                                                     */
/*********************************************************************************/

/*! \brief Return claimed CPUs of other processes
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be returned
 */
int DLB_Return(void);

/*! \brief Return the specific CPU if it has been reclaimed
 *  \param[in] cpuid CPU to return
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be returned
 */
int DLB_ReturnCpu(int cpuid);


/*********************************************************************************/
/*    DROM Responsive                                                            */
/*********************************************************************************/

/*! \brief Poll DROM module to check if the process needs to adapt to a new mask
 *          or number of threads
 *  \param[out] nthreads optional, variable to receive the new number of threads
 *  \param[out] mask optional, variable to receive the new mask
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_NOUPDT if no update id needed
 */
int DLB_PollDROM(int *nthreads, dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Misc                                                                       */
/*********************************************************************************/

/*! \brief Check whether the specified CPU is being used by another process
 *  \param[in] cpuid CPU to be checked
 *  \return DLB_SUCCESS if the CPU is available
 *  \return DLB_ERR_PERM if the CPU still has not been returned
 */
int DLB_CheckCpuAvailability(int cpuid);

/*! \brief Barrier between processes in the node
 *  \return DLB_SUCCESS on success
 */
int DLB_Barrier(void);

/*! \brief Change the value of a DLB internal variable
 *  \param[in] variable Internal variable to set
 *  \param[in] value New value
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_PERM if the variable is readonly
 *  \return DLB_ERR_NOENT if the variable does not exist
 */
int DLB_SetVariable(const char *variable, const char *value);

/*! \brief Query the value of a DLB internal variable
 *  \param[in] variable Internal variable to set
 *  \param[out] value Current DLB variable value
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOENT if the variable does not exist
 */
int DLB_GetVariable(const char *variable, char *value);

/*! \brief Print DLB internal variables
 *  \return DLB_SUCCESS on success
 */
int DLB_PrintVariables(void);

/*! \brief Print the data stored in the shmem
 *  \return DLB_SUCCESS on success
 */
int DLB_PrintShmem(void);

/*! \brief Obtaing a pointer to a string that describes the error code passed by argument
 *  \param[in] errnum error code to consult
 *  \return destription string pointer
 */
const char* DLB_Strerror(int errnum);


#ifdef __cplusplus
}
#endif

#endif /* DLB_H */
