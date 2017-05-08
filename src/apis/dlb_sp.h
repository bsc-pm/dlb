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

#ifndef DLB_SP_H
#define DLB_SP_H

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
 *  \return dlb_handler to use on subsequent DLB calls
 */
dlb_handler_t DLB_Init_sp(int ncpus, const_dlb_cpu_set_t mask, const char *dlb_args);

/*! \brief Finalize DLB library
 *  \return DLB_SUCCESS on success
 */
int DLB_Finalize_sp(dlb_handler_t handler);

/*! \brief Enable DLB
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *
 *  Only if the library was previously disabled, subsequent DLB calls will be
 *  attended again.
 */
int DLB_Enable_sp(dlb_handler_t handler);

/*! \brief Disable DLB
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *
 *  The process will recover the original status of its resources, and further DLB
 *  calls will be ignored.
 */
int DLB_Disable_sp(dlb_handler_t handler);

/*! \brief Set a maximum number of resources to be acquired by the calling process
 *  \param[in] handler subprocess identifier
 *  \param[in] max max number of CPUs
 *  \return DLB_SUCCESS on success
 */
int DLB_SetMaxParallelism_sp(dlb_handler_t handler, int max);


/*********************************************************************************/
/*    Callbacks                                                                  */
/*********************************************************************************/

/*! \brief Set callback
 *  \param[in] handler subprocess identifier
 *  \param[in] which callback type
 *  \param[in] callback function pointer to register
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOCBK if the callback type does not exist
 */
int DLB_CallbackSet_sp(dlb_handler_t handler, dlb_callbacks_t which, dlb_callback_t callback);

/*! \brief Get callback
 *  \param[in] handler subprocess identifier
 *  \param[in] which callback type
 *  \param[out] callback registered callback function for the specified callback type
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOCBK if the callback type does not exist
 */
int DLB_CallbackGet_sp(dlb_handler_t handler, dlb_callbacks_t which, dlb_callback_t *callback);


/*********************************************************************************/
/*    Lend                                                                       */
/*********************************************************************************/

/*! \brief Lend all the current CPUs
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 */
int DLB_Lend_sp(dlb_handler_t handler);

/*! \brief Lend a specific CPU
 *  \param[in] handler subprocess identifier
 *  \param[in] cpuid CPU id to lend
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 */
int DLB_LendCpu_sp(dlb_handler_t handler, int cpuid);

/*! \brief Lend a specific amount of CPUs, only useful for systems that do not
 *          work with cpu masks
 *  \param[in] handler subprocess identifier
 *  \param[in] ncpus number of CPUs to lend
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 */
int DLB_LendCpus_sp(dlb_handler_t handler, int ncpus);

/*! \brief Lend a set of CPUs
 *  \param[in] handler subprocess identifier
 *  \param[in] mask CPU mask to lend
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 */
int DLB_LendCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Reclaim                                                                    */
/*********************************************************************************/

/*! \brief Reclaim all the CPUs that are owned by the process
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 *  \return DLB_ERR_NOUPDT if there is no CPUs to reclaim
 */
int DLB_Reclaim_sp(dlb_handler_t handler);

/*! \brief Reclaim a specific CPU that are owned by the process
 *  \param[in] handler subprocess identifier
 *  \param[in] cpuid CPU id to reclaim
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be reclaimed
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 *  \return DLB_ERR_NOUPDT if there is no CPUs to reclaim
 */
int DLB_ReclaimCpu_sp(dlb_handler_t handler, int cpuid);

/*! \brief Reclaim a specific amount of CPUs that are owned by the process
 *  \param[in] handler subprocess identifier
 *  \param[in] ncpus Number of CPUs to reclaim
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be reclaimed
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 *  \return DLB_ERR_NOUPDT if there is no CPUs to reclaim
 */
int DLB_ReclaimCpus_sp(dlb_handler_t handler, int ncpus);

/*! \brief Reclaim a set of CPUs
 *  \param[in] handler subprocess identifier
 *  \param[in] mask CPU mask to reclaim
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be reclaimed
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 *  \return DLB_ERR_NOUPDT if there is no CPUs to reclaim
 */
int DLB_ReclaimCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Acquire                                                                    */
/*********************************************************************************/

/*! \brief Acquire a specific CPU
 *  \param[in] handler subprocess identifier
 *  \param[in] cpuid CPU to acquire
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be acquired
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 *  \return DLB_ERR_REQST if there are too many requests for this resource
 */
int DLB_AcquireCpu_sp(dlb_handler_t handler, int cpuid);

/*! \brief Acquire a set of CPUs
 *  \param[in] handler subprocess identifier
 *  \param[in] mask CPU set to acquire
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be acquired
 *  \return DLB_ERR_NOTED if the petition cannot be immediatelly fulfilled
 *  \return DLB_ERR_REQST if there are too many requests for these resources
 */
int DLB_AcquireCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Borrow                                                                     */
/*********************************************************************************/

/*! \brief Borrow all the possible CPUs registered on DLB
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if cannot borrow any resources
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 */
int DLB_Borrow_sp(dlb_handler_t handler);

/*! \brief Borrow a specific CPU
 *  \param[in] handler subprocess identifier
 *  \param[in] cpuid cpu CPU to borrow
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if CPU cannot borrowed
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 */
int DLB_BorrowCpu_sp(dlb_handler_t handler, int cpuid);

/*! \brief Borrow a specific amount of CPUs
 *  \param[in] handler subprocess identifier
 *  \param[in] ncpus Number of CPUs to borrow
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if cannot borrow any resources
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 */
int DLB_BorrowCpus_sp(dlb_handler_t handler, int ncpus);

/*! \brief Borrow a set of CPUs
 *  \param[in] handler subprocess identifier
 *  \param[in] mask CPU set to borrow
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if cannot borrow any resources
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 */
int DLB_BorrowCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Return                                                                     */
/*********************************************************************************/

/*! \brief Return claimed CPUs of other processes
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be returned
 */
int DLB_Return_sp(dlb_handler_t handler);

/*! \brief Return the specific CPU if it has been reclaimed
 *  \param[in] handler subprocess identifier
 *  \param[in] cpuid CPU to return
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be returned
 */
int DLB_ReturnCpu_sp(dlb_handler_t handler, int cpuid);

/*! \brief Return a set of CPUs if some of them have been reclaimed
 *  \param[in] handler subprocess identifier
 *  \param[in] mask CPU set to return
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be returned
 */
int DLB_ReturnCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    DROM Responsive                                                            */
/*********************************************************************************/

/*! \brief Poll DROM module to check if the process needs to adapt to a new mask
 *          or number of CPUs
 *  \param[in] handler subprocess identifier
 *  \param[out] ncpus optional, variable to receive the new number of CPUs
 *  \param[out] mask optional, variable to receive the new mask
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_NOUPDT if no update id needed
 */
int DLB_PollDROM_sp(dlb_handler_t handler, int *ncpus, dlb_cpu_set_t mask);

/*! \brief Poll DROM module to check if the process needs to adapt to a new mask
 *          and update it if necessary using the registered callbacks
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_NOUPDT if no update id needed
 */
int DLB_PollDROM_Update_sp(dlb_handler_t handler);


/*********************************************************************************/
/*    Misc                                                                       */
/*********************************************************************************/

/*! \brief Change the value of a DLB internal variable
 *  \param[in] handler subprocess identifier
 *  \param[in] variable Internal variable to set
 *  \param[in] value New value
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_PERM if the variable is readonly
 *  \return DLB_ERR_NOENT if the variable does not exist
 */
int DLB_SetVariable_sp(dlb_handler_t handler, const char *variable, const char *value);

/*! \brief Query the value of a DLB internal variable
 *  \param[in] handler subprocess identifier
 *  \param[in] variable Internal variable to set
 *  \param[out] value Current DLB variable value
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOENT if the variable does not exist
 */
int DLB_GetVariable_sp(dlb_handler_t handler, const char *variable, char *value);

/*! \brief Print DLB internal variables
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 */
int DLB_PrintVariables_sp(dlb_handler_t handler);


#ifdef __cplusplus
}
#endif

#endif /* DLB_SP_H */
