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

#ifndef DLB_DROM_H
#define DLB_DROM_H

#include "dlb_types.h"
#include "dlb_errors.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************************/
/*    Dynamic Resource Manager Module                                            */
/*********************************************************************************/

/*! \brief Attach current process to DLB system as DROM administrator
 *  \return DLB_SUCCESS on success
 *
 *  Once the process is attached to DLB as DROM administrator, it may perform the
 *  below actions described in this file. This way, the process is able to query
 *  or modify other DLB running processes' IDs and processes masks, as well as to
 *  make room in the system for creating another running process.
 */
int DLB_DROM_Attach(void);

/*! \brief Detach current process from DLB system
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory to dettach from
 *
 *  If previously attached, a process must call this function to correctly close
 *  file descriptors and clean data.
 */
int DLB_DROM_Detach(void);

/*! \brief Get the number of CPUs in the node
 *  \param[out] ncpus the number of CPUs
 *  \return DLB_SUCCESS on success
 */
int DLB_DROM_GetNumCpus(int *ncpus);

/*! \brief Get the list of running processes registered in the DLB system
 *  \param[out] pidlist The output list
 *  \param[out] nelems Number of elements in the list
 *  \param[in] max_len Max capacity of the list
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory
 */
int DLB_DROM_GetPidList(int *pidlist, int *nelems, int max_len);

/*! \brief Get the process mask of the given PID
 *  \param[in] pid Process ID to query its process mask, or 0 if current process
 *  \param[out] mask Current process mask of the target process
 *  \param[in] flags DROM options
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOTED if a new mask is given for the current process (replaces PollDROM)
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory
 *  \return DLB_ERR_NOPROC if target pid is not registered in the DLB system
 *  \return DLB_ERR_TIMEOUT if the query is synchronous and times out
 *
 *  Accepted flags for this function:\n
 *      DLB_SYNC_QUERY: Synchronous query. If the target process has any pending
 *                      operations, the caller process gets blocked until the target
 *                      process resolves them, or the query times out.
 */
int DLB_DROM_GetProcessMask(int pid, dlb_cpu_set_t mask, dlb_drom_flags_t flags);

/*! \brief Set the process mask of the given PID
 *  \param[in] pid Target Process ID to apply a new process mask, or 0 if current process
 *  \param[in] mask Process mask to set
 *  \param[in] flags DROM options
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory
 *  \return DLB_ERR_NOPROC if target pid is not registered in the DLB system
 *  \return DLB_ERR_PDIRTY if target pid already has a pending operation
 *  \return DLB_ERR_TIMEOUT if the query is synchronous and times out
 *  \return DLB_ERR_PERM if the provided mask could not be stolen
 *
 *  Accepted flags for this function:\n
 *      DLB_SYNC_QUERY: Synchronous query. If the target process has any pending
 *                      operations, the caller process gets blocked until the target
 *                      process resolves them, or the query times out.\n
 *      DLB_SYNC_NOW:   Force mask synchronization if target is the current process.
 *                      If pid is 0 or current process id, and setting the new mask
 *                      succeeds, this operation automatically forces the synchronization
 *                      with the set_process_mask callback. It has the equivalent
 *                      behaviour as invoking DLB_PollDROM_Update() after a
 *                      successful operation.
 */
int DLB_DROM_SetProcessMask(int pid, const_dlb_cpu_set_t mask, dlb_drom_flags_t flags);

/*! \brief Make room in the system for a new process with the given mask
 *  \param[in] pid Process ID that gets the reservation
 *  \param[in] mask Process mask to register
 *  \param[in] flags DROM options
 *  \param[inout] next_environ environment to modify if the process is going to fork-exec
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory
 *  \return DLB_ERR_INIT if the process is already registered
 *  \return DLB_ERR_TIMEOUT if the query is synchronous and times out
 *  \return DLB_ERR_PERM if the provided mask overlaps with an existing registered
 *                      process and stealing option is not set
 *
 *  Accepted flags for this function:\n
 *      DLB_STEAL_CPUS:     Steal CPU ownership if necessary. If any CPU in mask is already
 *                          registered in the system, steal the ownership from their
 *                          original process.\n
 *      DLB_RETURN_STOLEN:  Return stolen CPUs when this process finalizes. If any CPU
 *                          was stolen during the preinitialization, try to return those
 *                          CPUs to their owners if they still exist.\n
 *      DLB_SYNC_QUERY:     Synchronous query. If the preinitialization needs to steal
 *                          some CPU, the stealing operation is synchronous and thus will
 *                          wait until the target process can release its CPU instead of
 *                          causing oversubscription. This option may cause a time out
 *                          if the target process does not update its mask in time.
 *
 *  This function can be called to preinitialize a future DLB running process, even
 *  if the current process ID does not match the future process, probably due to
 *  fork-exec mechanisms. Though in this case we need to modify the environment in order
 *  to not loose the preinitialization info.
 *
 *  Even if preinialized, a running process still needs to call DLB_Init() and
 *  DLB_Finalize() to take full advantage of all DLB features, but it is not mandatory.
 */
int DLB_DROM_PreInit(int pid, const_dlb_cpu_set_t mask, dlb_drom_flags_t flags,
        char ***next_environ);

/*! \brief Unregister a process from the DLB system
 *  \param[in] pid Process ID to unregister
 *  \param[in] flags DROM options
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory
 *  \return DLB_ERR_NOPROC if target pid is not registered in the DLB system
 *
 *  Accepted flags for this function:\n
 *      DLB_RETURN_STOLEN:  Return stolen CPUs when this process finalizes. If any CPU
 *                          was stolen during the preinitialization, try to return those
 *                          CPUs to their owners if they still exist.
 *
 *  This function should be called after a preinitialized child process has finished
 *  its execution.
 *
 *  If the child process was DLB aware and called DLB_Init() and DLB_Finalize(), this
 *  function will return DLB_ERR_NOPROC, although that should not be considered as a
 *  wrong scenario.
 *
 */
int DLB_DROM_PostFinalize(int pid, dlb_drom_flags_t flags);

/*! \brief Recover previously stolen CPUs if they are idle
 *  \param[in] pid Process ID
 *  \return DLB_SUCCESS on success
 *  \return DLB_NOUPDT if the given process has not stolen CPUs
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory
 *  \return DLB_ERR_NOPROC if target pid is not registered in the DLB system
 *
 *  If a running process abandoned the system without returning the previously stolen
 *  CPUs, this function may be called at any time to check if the target process is
 *  able to recover some of its original CPUs.
 */
int DLB_DROM_RecoverStolenCpus(int pid);

#ifdef __cplusplus
}
#endif

#endif /* DLB_DROM_H */
