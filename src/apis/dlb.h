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

/*! \brief Initialize DLB library and all its internal data structures.
 *          Must be called once and only once by each process in the DLB system.
 *  \param[in] ncpus optional, initial number of CPUs, or 0
 *  \param[in] mask optional, initial CPU mask to register, or NULL
 *  \param[in] dlb_args optional parameter to overwrite DLB_ARGS options, or NULL
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_INIT if DLB is already initialized
 *  \return DLB_ERR_PERM if DLB cannot register the mask passed by argument
 *  \return DLB_ERR_NOMEM if DLB cannot allocate more processes
 *
 *  Parameters \p ncpus and \p mask are used to register CPUs owned by the
 *  calling process into the system. DLB advanced usage requires mask
 *  information so it is recommended to provide a CPU mask, but DLB also
 *  accepts an integer in case the program does not have the mask affinity
 *  details. Parameter \p dlb_args can be used in conjunction with DLB_ARGS,
 *  the former takes precedence in case of conflicting options.
 */
int DLB_Init(int ncpus, const_dlb_cpu_set_t mask, const char *dlb_args);

/*! \brief Finalize DLB library and clean up all its data structures.
 *          Must be called by each process before exiting the system.
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 */
int DLB_Finalize(void);

/*! \brief Pre-Initialize process
 *  \param[in] mask initial CPU mask to register
 *  \param[inout] next_environ environment to modify if the process is going to fork-exec
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_PERM if DLB cannot register the mask passed by argument
 *  \return DLB_ERR_NOMEM if DLB cannot allocate more processes
 */
int DLB_PreInit(const_dlb_cpu_set_t mask, char ***next_environ);

/*! \brief Enable DLB and all its features in case it was previously disabled,
 *          otherwise it has no effect.
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if DLB is already enabled
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *
 *  It can be used in conjunction with DLB_Disable() to delimit sections of
 *  the code where DLB calls will not have effect.
 */
int DLB_Enable(void);

/*! \brief Disable DLB actions for the calling process.
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if DLB is already disabled
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *
 *  This call resets the original resources for the process and returns any
 *  external CPU it may be using at that time. While DLB is disabled there will
 *  not be any resource sharing for this process.
 */
int DLB_Disable(void);

/*! \brief Set the maximum number of resources to be used by the calling process.
 *  \param[in] max max number of CPUs
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *
 *  Used to delimit sections of the code that the developer knows that only a
 *  maximum number of CPUs can benefit the execution. If a process reaches its
 *  maximum number of resources used at any time, subsequent calls to borrow
 *  CPUs will be ignored until some of them are returned. If the maximum number
 *  of CPUs exceeds the current number of assigned CPUs at the time of this
 *  function call, DLB will readjust as needed.
 */
int DLB_SetMaxParallelism(int max);

/*! \brief Unset the maximum number of resources to be used by the calling process.
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *
 *  Unset the maximum number of CPUs previously assigned to this process.
 *  Subsequent calls to borrow will not be delimited by this parameter.
 */
int DLB_UnsetMaxParallelism(void);


/*********************************************************************************/
/*    Callbacks                                                                  */
/*********************************************************************************/

/*! \brief Setter for DLB callbacks
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
int DLB_CallbackSet(dlb_callbacks_t which, dlb_callback_t callback, void *arg);

/*! \brief Getter for DLB callbacks
 *  \param[in] which callback type
 *  \param[out] callback registered callback function for the specified callback type
 *  \param[out] arg opaque argument to pass in each callback invocation
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOCBK if the callback type does not exist
 *
 *  Obtain the previously registered \p callback and \p arg for the specified
 *  \p which callback type.
 */
int DLB_CallbackGet(dlb_callbacks_t which, dlb_callback_t *callback, void **arg);


/*********************************************************************************/
/*    Lend                                                                       */
/*********************************************************************************/

/*! \brief Lend all the current CPUs
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Lend CPUs of the process to the system. A lent CPU may be assigned to other
 *  process that demands more resources. If the CPU was originally owned by the
 *  process it may be reclaimed.
 */
int DLB_Lend(void);

/*! \brief Lend a specific CPU
 *  \param[in] cpuid CPU id to lend
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Lend CPUs of the process to the system. A lent CPU may be assigned to other
 *  process that demands more resources. If the CPU was originally owned by the
 *  process it may be reclaimed.
 */
int DLB_LendCpu(int cpuid);

/*! \brief Lend a specific amount of CPUs, only useful for systems that do not
 *          work with cpu masks
 *  \param[in] ncpus number of CPUs to lend
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Lend CPUs of the process to the system. A lent CPU may be assigned to other
 *  process that demands more resources. If the CPU was originally owned by the
 *  process it may be reclaimed.
 */
int DLB_LendCpus(int ncpus);

/*! \brief Lend a set of CPUs
 *  \param[in] mask CPU mask to lend
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Lend CPUs of the process to the system. A lent CPU may be assigned to other
 *  process that demands more resources. If the CPU was originally owned by the
 *  process it may be reclaimed.
 */
int DLB_LendCpuMask(const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Reclaim                                                                    */
/*********************************************************************************/

/*! \brief Reclaim all the CPUs that are owned by the process
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_NOUPDT if there is no CPUs to reclaim
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Reclaim CPUs that were previously lent. It is mandatory that the CPUs
 *  belong to the calling process.
 */
int DLB_Reclaim(void);

/*! \brief Reclaim a specific CPU that are owned by the process
 *  \param[in] cpuid CPU id to reclaim
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_NOUPDT if there is no CPUs to reclaim
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be reclaimed
 *
 *  Reclaim CPUs that were previously lent. It is mandatory that the CPUs
 *  belong to the calling process.
 */
int DLB_ReclaimCpu(int cpuid);

/*! \brief Reclaim a specific amount of CPUs that are owned by the process
 *  \param[in] ncpus Number of CPUs to reclaim
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_NOUPDT if there is no CPUs to reclaim
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Reclaim CPUs that were previously lent. It is mandatory that the CPUs
 *  belong to the calling process.
 */
int DLB_ReclaimCpus(int ncpus);

/*! \brief Reclaim a set of CPUs
 *  \param[in] mask CPU mask to reclaim
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_NOUPDT if there is no CPUs to reclaim
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be reclaimed
 *
 *  Reclaim CPUs that were previously lent. It is mandatory that the CPUs
 *  belong to the calling process.
 */
int DLB_ReclaimCpuMask(const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Acquire                                                                    */
/*********************************************************************************/

/*! \brief Acquire a specific CPU
 *  \param[in] cpuid CPU to acquire
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_NOUPDT if the CPU is already acquired
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be acquired
 *  \return DLB_ERR_REQST if there are too many requests for this resource
 *
 *  Acquire CPUs from the system. If the CPU belongs to the process the call is
 *  equivalent to a *reclaim* action. Otherwise the process attempts to acquire
 *  a specific CPU in case it is available or enqueue a request if it's not.
 */
int DLB_AcquireCpu(int cpuid);

/*! \brief Acquire a specific amount of CPUs.
 *  \param[in] ncpus Number of CPUs to acquire
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_NOUPDT if cannot acquire any CPU
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_REQST if there are too many requests for this resource
 *
 *  Acquire CPUs from the system. If the CPU belongs to the process the call is
 *  equivalent to a *reclaim* action. Otherwise the process attempts to acquire
 *  a specific CPU in case it is available or enqueue a request if it's not.
 */
int DLB_AcquireCpus(int ncpus);

/*! \brief Acquire a set of CPUs
 *  \param[in] mask CPU set to acquire
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOTED if the petition cannot be immediately fulfilled
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be acquired
 *  \return DLB_ERR_REQST if there are too many requests for these resources
 *
 *  Acquire CPUs from the system. If the CPU belongs to the process the call is
 *  equivalent to a *reclaim* action. Otherwise the process attempts to acquire
 *  a specific CPU in case it is available or enqueue a request if it's not.
 */
int DLB_AcquireCpuMask(const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    Borrow                                                                     */
/*********************************************************************************/

/*! \brief Borrow all the possible CPUs registered on DLB
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if cannot borrow any resources
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Borrow CPUs from the system only if they are idle. No other action is done
 *  if the CPU is not available.
 */
int DLB_Borrow(void);

/*! \brief Borrow a specific CPU
 *  \param[in] cpuid cpu CPU to borrow
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if CPU cannot borrowed
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Borrow CPUs from the system only if they are idle. No other action is done
 *  if the CPU is not available.
 */
int DLB_BorrowCpu(int cpuid);

/*! \brief Borrow a specific amount of CPUs
 *  \param[in] ncpus Number of CPUs to borrow
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if cannot borrow any resources
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Borrow CPUs from the system only if they are idle. No other action is done
 *  if the CPU is not available.
 */
int DLB_BorrowCpus(int ncpus);

/*! \brief Borrow a set of CPUs
 *  \param[in] mask CPU set to borrow
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if cannot borrow any resources
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Borrow CPUs from the system only if they are idle. No other action is done
 *  if the CPU is not available.
 */
int DLB_BorrowCpuMask(const_dlb_cpu_set_t mask);

/*********************************************************************************/
/*    Return                                                                     */
/*********************************************************************************/

/*! \brief Return claimed CPUs of other processes
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if no need to return
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be returned
 *
 *  Return CPUs to the system commonly triggered by a reclaim action from other
 *  process but stating that the current process still demands the usage of
 *  these CPUs. This action will enqueue a request for when the resources are
 *  available again.  If the caller does not want to keep the resource after
 *  receiving a *reclaim*, the correct action is *lend*.
 */
int DLB_Return(void);

/*! \brief Return the specific CPU if it has been reclaimed
 *  \param[in] cpuid CPU to return
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOINIT if DLB is not initialized
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be returned
 *
 *  Return CPUs to the system commonly triggered by a reclaim action from other
 *  process but stating that the current process still demands the usage of
 *  these CPUs. This action will enqueue a request for when the resources are
 *  available again.  If the caller does not want to keep the resource after
 *  receiving a *reclaim*, the correct action is *lend*.
 */
int DLB_ReturnCpu(int cpuid);

/*! \brief Return a set of CPUs if some of them have been reclaimed
 *  \param[in] mask CPU set to return
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *  \return DLB_ERR_PERM if the resources cannot be returned
 *
 *  Return CPUs to the system commonly triggered by a reclaim action from other
 *  process but stating that the current process still demands the usage of
 *  these CPUs. This action will enqueue a request for when the resources are
 *  available again.  If the caller does not want to keep the resource after
 *  receiving a *reclaim*, the correct action is *lend*.
 */
int DLB_ReturnCpuMask(const_dlb_cpu_set_t mask);


/*********************************************************************************/
/*    DROM Responsive                                                            */
/*********************************************************************************/

/*! \brief Poll DROM module to check if the process needs to adapt to a new mask
 *          or number of CPUs
 *  \param[out] ncpus optional, variable to receive the new number of CPUs
 *  \param[out] mask optional, variable to receive the new mask
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if no update id needed
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  If DROM is enabled and the interaction mode is not asynchronous, this
 *  function can be called to poll the status of the CPU ownership.
 */
int DLB_PollDROM(int *ncpus, dlb_cpu_set_t mask);

/*! \brief Poll DROM module to check if the process needs to adapt to a new mask
 *          and update it if necessary using the registered callbacks
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if no update id needed
 *  \return DLB_ERR_DISBLD if DLB is disabled
 *
 *  Same as DLB_PollDROM(), but calling the registered callbacks to update the
 *  ownership info instead of returning the data by argument.
 */
int DLB_PollDROM_Update(void);


/*********************************************************************************/
/*    Misc                                                                       */
/*********************************************************************************/

/*! \brief Check whether the specified CPU is being used by another process
 *  \param[in] cpuid CPU to be checked
 *  \return DLB_SUCCESS if the CPU is available
 *  \return DLB_NOTED if the CPU is owned but still guested by other process
 *  \return DLB_NOUPDT if the CPU is owned but still not reclaimed
 *  \return DLB_ERR_PERM if the CPU cannot be acquired or has been disabled
 *  \return DLB_ERR_DISBLD if DLB is disabled
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
 *
 *  Set a DLB internal variable. These variables are the same ones
 *  specified in DLB_ARGS, although not all of them can be modified at runtime.
 *  If the variable is readonly the setter function will return an error.
 */
int DLB_SetVariable(const char *variable, const char *value);

/*! \brief Query the value of a DLB internal variable
 *  \param[in] variable Internal variable to set
 *  \param[out] value Current DLB variable value
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOENT if the variable does not exist
 *
 *  Get a DLB internal variable. See DLB_SetVariable().
 */
int DLB_GetVariable(const char *variable, char *value);

/*! \brief Print DLB internal variables
 *  \param[in] print_extended If different to 0, print all options,
 *              including experimental, and its description
 *  \return DLB_SUCCESS on success
 */
int DLB_PrintVariables(int print_extended);

/*! \brief Print the data stored in the shmem
 *  \param[in] num_columns Number of columns to use when printing
 *  \param[in] print_flags Print options
 *  \return DLB_SUCCESS on success
 */
int DLB_PrintShmem(int num_columns, dlb_printshmem_flags_t print_flags);

/*! \brief Obtain a pointer to a string that describes the error code passed by argument
 *  \param[in] errnum error code to consult
 *  \return pointer to string with the error description
 */
const char* DLB_Strerror(int errnum);


#ifdef __cplusplus
}
#endif

#endif /* DLB_H */
