/*********************************************************************************/
/*  Copyright 2009-2018 Barcelona Supercomputing Center                          */
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

/*! \brief Initialize DLB library and all its internal data structures.
 *  \param[in] ncpus optional, initial number of CPUs, or 0
 *  \param[in] mask optional, initial CPU mask to register, or NULL
 *  \param[in] dlb_args optional parameter to overwrite DLB_ARGS options, or NULL
 *  \return dlb_handler to use on subsequent DLB calls
 *
 *  Parameters \p ncpus and \p mask are used to register CPUs owned by the
 *  calling subprocess into the system. DLB advanced usage requires mask
 *  information so it is recommended to provide a CPU mask, but DLB also
 *  accepts an integer in case the program does not have the mask affinity
 *  details. Parameter \p dlb_args can be used in conjunction with DLB_ARGS,
 *  the former takes precedence in case of conflicting options.
 */
dlb_handler_t DLB_Init_sp(int ncpus, const_dlb_cpu_set_t mask, const char *dlb_args);

/*! \brief Finalize DLB library and clean up all its data structures for the
 *          specified handler. Must be called once by each created handler
 *          before exiting the system.
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 */
int DLB_Finalize_sp(dlb_handler_t handler);

/*! \brief Enable DLB and all its features in case it was previously disabled,
 *          otherwise it has no effect.
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if DLB is already enabled
 *
 *  It can be used in conjunction with DLB_Disable() to delimit sections of
 *  the code where DLB calls will not have effect.
 */
int DLB_Enable_sp(dlb_handler_t handler);

/*! \brief Disable DLB actions for the specified handler.
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if DLB is already disabled
 *
 *  This call resets the original resources for the subprocess and returns any
 *  external CPU it may be using at that time. While DLB is disabled there will
 *  not be any resource sharing for this subprocess.
 */
int DLB_Disable_sp(dlb_handler_t handler);

/*! \brief Set the maximum number of resources to be used by the handler.
 *  \param[in] handler subprocess identifier
 *  \param[in] max max number of CPUs
 *  \return DLB_SUCCESS on success
 *  \todo this feature is experimental and it's only implemented for non-mask LeWI policy
 *
 *  Useful to delimit sections of the code that the developer knows that only a
 *  maximum number of CPUs can benefit the execution. If a subprocess reaches its
 *  maximum number of resources used at any time, subsequent calls to borrow
 *  CPUs will be ignored until some of them are returned.

 */
int DLB_SetMaxParallelism_sp(dlb_handler_t handler, int max);


/*********************************************************************************/
/*    Callbacks                                                                  */
/*********************************************************************************/

/*! \brief Setter for DLB callbacks
 *  \param[in] handler subprocess identifier
 *  \param[in] which callback type
 *  \param[in] callback function pointer to register
 *  \param[in] arg opaque argument to pass in each callback invocation
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOCBK if the callback type does not exist
 *
 *  Register a new \p callback for the callback type \p which. The callback
 *  type comes predefined by the enum values of ::dlb_callbacks_t.  It is
 *  highly recommended to register at least callbacks for
 *  ::dlb_callback_enable_cpu and ::dlb_callback_disable_cpu.
 */
int DLB_CallbackSet_sp(dlb_handler_t handler, dlb_callbacks_t which,
        dlb_callback_t callback, void *arg);

/*! \brief Getter for DLB callbacks
 *  \param[in] handler subprocess identifier
 *  \param[in] which callback type
 *  \param[out] callback registered callback function for the specified callback type
 *  \param[out] arg opaque argument to pass in each callback invocation
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOCBK if the callback type does not exist
 *
 *  Obtain the previously registered \p callback and \p arg for the specified
 *  \p which callback type.
 */
int DLB_CallbackGet_sp(dlb_handler_t handler, dlb_callbacks_t which,
        dlb_callback_t *callback, void **arg);


/*********************************************************************************/
/*    Lend                                                                       */
/*********************************************************************************/

/*! \brief Lend all the current CPUs
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Lend CPUs of the subprocess to the system. A lent CPU may be assigned to other
 *  subprocess that demands more resources. If the CPU was originally owned by the
 *  subprocess it may be reclaimed.
 */
int DLB_Lend_sp(dlb_handler_t handler);

/*! \brief Lend a specific CPU
 *  \param[in] handler subprocess identifier
 *  \param[in] cpuid CPU id to lend
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Lend CPUs of the subprocess to the system. A lent CPU may be assigned to other
 *  subprocess that demands more resources. If the CPU was originally owned by the
 *  subprocess it may be reclaimed.
 */
int DLB_LendCpu_sp(dlb_handler_t handler, int cpuid);

/*! \brief Lend a specific amount of CPUs, only useful for systems that do not
 *          work with cpu masks
 *  \param[in] handler subprocess identifier
 *  \param[in] ncpus number of CPUs to lend
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Lend CPUs of the subprocess to the system. A lent CPU may be assigned to other
 *  subprocess that demands more resources. If the CPU was originally owned by the
 *  subprocess it may be reclaimed.
 */
int DLB_LendCpus_sp(dlb_handler_t handler, int ncpus);

/*! \brief Lend a set of CPUs
 *  \param[in] handler subprocess identifier
 *  \param[in] mask CPU mask to lend
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Lend CPUs of the subprocess to the system. A lent CPU may be assigned to other
 *  subprocess that demands more resources. If the CPU was originally owned by the
 *  subprocess it may be reclaimed.
 */
int DLB_LendCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Reclaim                                                                    */
/*********************************************************************************/

/*! \brief Reclaim all the CPUs that are owned by the subprocess
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_ERR_NOUPDT if there is no CPUs to reclaim
 *
 *  Reclaim CPUs that were previously lent. It is mandatory that the CPUs
 *  belong to the calling subprocess.
 */
int DLB_Reclaim_sp(dlb_handler_t handler);

/*! \brief Reclaim a specific CPU that are owned by the subprocess
 *  \param[in] handler subprocess identifier
 *  \param[in] cpuid CPU id to reclaim
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be reclaimed
 *  \return DLB_ERR_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_ERR_NOUPDT if there is no CPUs to reclaim
 *
 *  Reclaim CPUs that were previously lent. It is mandatory that the CPUs
 *  belong to the calling subprocess.
 */
int DLB_ReclaimCpu_sp(dlb_handler_t handler, int cpuid);

/*! \brief Reclaim a specific amount of CPUs that are owned by the subprocess
 *  \param[in] handler subprocess identifier
 *  \param[in] ncpus Number of CPUs to reclaim
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_ERR_NOUPDT if there is no CPUs to reclaim
 *
 *  Reclaim CPUs that were previously lent. It is mandatory that the CPUs
 *  belong to the calling subprocess.
 */
int DLB_ReclaimCpus_sp(dlb_handler_t handler, int ncpus);

/*! \brief Reclaim a set of CPUs
 *  \param[in] handler subprocess identifier
 *  \param[in] mask CPU mask to reclaim
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be reclaimed
 *  \return DLB_ERR_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_ERR_NOUPDT if there is no CPUs to reclaim
 *
 *  Reclaim CPUs that were previously lent. It is mandatory that the CPUs
 *  belong to the calling subprocess.
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
 *  \return DLB_ERR_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_ERR_REQST if there are too many requests for this resource
 *
 *  Acquire CPUs from the system. If the CPU belongs to the subprocess the call is
 *  equivalent to a *reclaim* action. Otherwise the subprocess attempts to acquire
 *  a specific CPU in case it is available or enqueue a request if it's not.
 */
int DLB_AcquireCpu_sp(dlb_handler_t handler, int cpuid);

/*! \brief Acquire a specific amount of CPUs.
 *  \param[in] handler subprocess identifier
 *  \param[in] ncpus Number of CPUs to acquire
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_NOUPDT if cannot acquire any CPU
 *  \return DLB_ERR_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_ERR_REQST if there are too many requests for this resource
 *
 *  Acquire CPUs from the system. If the CPU belongs to the subprocess the call is
 *  equivalent to a *reclaim* action. Otherwise the subprocess attempts to acquire
 *  a specific CPU in case it is available or enqueue a request if it's not.
 */
int DLB_AcquireCpus_sp(dlb_handler_t handler, int ncpus);

/*! \brief Acquire a set of CPUs
 *  \param[in] handler subprocess identifier
 *  \param[in] mask CPU set to acquire
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be acquired
 *  \return DLB_ERR_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_ERR_REQST if there are too many requests for these resources
 *
 *  Acquire CPUs from the system. If the CPU belongs to the subprocess the call is
 *  equivalent to a *reclaim* action. Otherwise the subprocess attempts to acquire
 *  a specific CPU in case it is available or enqueue a request if it's not.
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
 *
 *  Borrow CPUs from the system only if they are idle. No other action is done
 *  if the CPU is not available.
 */
int DLB_Borrow_sp(dlb_handler_t handler);

/*! \brief Borrow a specific CPU
 *  \param[in] handler subprocess identifier
 *  \param[in] cpuid cpu CPU to borrow
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if CPU cannot borrowed
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Borrow CPUs from the system only if they are idle. No other action is done
 *  if the CPU is not available.
 */
int DLB_BorrowCpu_sp(dlb_handler_t handler, int cpuid);

/*! \brief Borrow a specific amount of CPUs
 *  \param[in] handler subprocess identifier
 *  \param[in] ncpus Number of CPUs to borrow
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if cannot borrow any resources
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Borrow CPUs from the system only if they are idle. No other action is done
 *  if the CPU is not available.
 */
int DLB_BorrowCpus_sp(dlb_handler_t handler, int ncpus);

/*! \brief Borrow a set of CPUs
 *  \param[in] handler subprocess identifier
 *  \param[in] mask CPU set to borrow
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if cannot borrow any resources
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Borrow CPUs from the system only if they are idle. No other action is done
 *  if the CPU is not available.
 */
int DLB_BorrowCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Return                                                                     */
/*********************************************************************************/

/*! \brief Return claimed CPUs of other subprocesses
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be returned
 *
 *  Return CPUs to the system commonly triggered by a reclaim action from other
 *  subprocess but stating that the current subprocess still demands the usage of
 *  these CPUs. This action will enqueue a request for when the resources are
 *  available again.  If the caller does not want to keep the resource after
 *  receiving a *reclaim*, the correct action is *lend*.
 */
int DLB_Return_sp(dlb_handler_t handler);

/*! \brief Return the specific CPU if it has been reclaimed
 *  \param[in] handler subprocess identifier
 *  \param[in] cpuid CPU to return
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be returned
 *
 *  Return CPUs to the system commonly triggered by a reclaim action from other
 *  subprocess but stating that the current subprocess still demands the usage of
 *  these CPUs. This action will enqueue a request for when the resources are
 *  available again.  If the caller does not want to keep the resource after
 *  receiving a *reclaim*, the correct action is *lend*.
 */
int DLB_ReturnCpu_sp(dlb_handler_t handler, int cpuid);

/*! \brief Return a set of CPUs if some of them have been reclaimed
 *  \param[in] handler subprocess identifier
 *  \param[in] mask CPU set to return
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be returned
 *
 *  Return CPUs to the system commonly triggered by a reclaim action from other
 *  subprocess but stating that the current subprocess still demands the usage of
 *  these CPUs. This action will enqueue a request for when the resources are
 *  available again.  If the caller does not want to keep the resource after
 *  receiving a *reclaim*, the correct action is *lend*.
 */
int DLB_ReturnCpuMask_sp(dlb_handler_t handler, const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    DROM Responsive                                                            */
/*********************************************************************************/

/*! \brief Poll DROM module to check if the subprocess needs to adapt to a new mask
 *          or number of CPUs
 *  \param[in] handler subprocess identifier
 *  \param[out] ncpus optional, variable to receive the new number of CPUs
 *  \param[out] mask optional, variable to receive the new mask
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_NOUPDT if no update id needed
 *
 *  If DROM is enabled and the interaction mode is not asynchronous, this
 *  function can be called to poll the status of the CPU ownership.
 */
int DLB_PollDROM_sp(dlb_handler_t handler, int *ncpus, dlb_cpu_set_t mask);

/*! \brief Poll DROM module to check if the subprocess needs to adapt to a new mask
 *          and update it if necessary using the registered callbacks
 *  \param[in] handler subprocess identifier
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_NOUPDT if no update id needed
 *
 *  Same as DLB_PollDROM(), but calling the registered callbacks to update the
 *  ownership info instead of returning the data by argument.
 */
int DLB_PollDROM_Update_sp(dlb_handler_t handler);


/*********************************************************************************/
/*    Misc                                                                       */
/*********************************************************************************/

/*! \brief Check whether the specified CPU is being used by another subprocess
 *  \param[in] handler subprocess identifier
 *  \param[in] cpuid CPU to be checked
 *  \return DLB_SUCCESS if the CPU is available
 *  \return DLB_NOTED if the CPU is owned but still guested by other process
 *  \return DLB_ERR_PERM if the CPU cannot be acquired or has been disabled
 *  \return DLB_ERR_DISBLD if DLB is disabled
 */
int DLB_CheckCpuAvailability_sp(dlb_handler_t handler, int cpuid);

/*! \brief Change the value of a DLB internal variable
 *  \param[in] handler subprocess identifier
 *  \param[in] variable Internal variable to set
 *  \param[in] value New value
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_PERM if the variable is readonly
 *  \return DLB_ERR_NOENT if the variable does not exist
 *
 *  Set a DLB internal variable. These variables are the same ones
 *  specified in DLB_ARGS, although not all of them can be modified at runtime.
 *  If the variable is readonly the setter function will return an error.
 */
int DLB_SetVariable_sp(dlb_handler_t handler, const char *variable, const char *value);

/*! \brief Query the value of a DLB internal variable
 *  \param[in] handler subprocess identifier
 *  \param[in] variable Internal variable to set
 *  \param[out] value Current DLB variable value
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOENT if the variable does not exist
 *
 *  Get a DLB internal variable. See DLB_SetVariable().
 */
int DLB_GetVariable_sp(dlb_handler_t handler, const char *variable, char *value);

/*! \brief Print DLB internal variables
 *  \param[in] handler subprocess identifier
 *  \param[in] print_extra If different to 0, print all options,
 *              including experimental, and its description
 *  \return DLB_SUCCESS on success
 */
int DLB_PrintVariables_sp(dlb_handler_t handler, int print_extra);


#ifdef __cplusplus
}
#endif

#endif /* DLB_SP_H */
