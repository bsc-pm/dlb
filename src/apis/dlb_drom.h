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

/*! \brief Initialize DROM Module
 *  \return error code
 */
int DLB_DROM_Init(void);

/*! \brief Finalize DROM Module
 *  \return error code
 */
int DLB_DROM_Finalize(void);

/*! \brief Get the total number of available CPUs in the node
 *  \param[out] ncpus the number of CPUs
 *  \return error code
 */
int DLB_DROM_GetNumCpus(int *ncpus);

/*! \brief Get the PID's attached to this module
 *  \param[out] pidlist The output list
 *  \param[out] nelems Number of elements in the list
 *  \param[in] max_len Max capacity of the list
 *  \return error code
 */
int DLB_DROM_GetPidList(int *pidlist, int *nelems, int max_len);

/*! \brief Get the process mask of the given PID
 *  \param[in] pid Process ID to consult
 *  \param[out] mask Current process mask
 *  \return error code
 */
int DLB_DROM_GetProcessMask(int pid, dlb_cpu_set_t mask);

/*! \brief Set the process mask of the given PID
 *  \param[in] pid Process ID to signal
 *  \param[in] mask Process mask to set
 *  \return error code
 */
int DLB_DROM_SetProcessMask(int pid, const_dlb_cpu_set_t mask);

/*! \brief Get the process mask of the given PID
 *  \param[in] pid Process ID to consult
 *  \param[out] mask Current process mask
 *  \return error code
 */
int DLB_DROM_GetProcessMask_sync(int pid, dlb_cpu_set_t mask);

/*! \brief Set the process mask of the given PID
 *  \param[in] pid Process ID to signal
 *  \param[in] mask Process mask to set
 *  \return error code
 */
int DLB_DROM_SetProcessMask_sync(int pid, const_dlb_cpu_set_t mask);

/*! \brief Register PID with the given mask before the process normal registration
 *  \param[in] pid Process ID that gets the reservation
 *  \param[in] mask Process mask to register
 *  \param[in] steal whether to steal owned CPUs
 *  \param[inout] environ environment to modify if the subprocess may be able to fork
 *  \return error code
 */
int DLB_DROM_PreInit(int pid, const_dlb_cpu_set_t mask, int steal, char ***environ);

/*! \brief Finalize process
 *  \param[in] pid Process ID
 *  \param[in] return_stolen whether to return stolen CPUs
 *  \return error code
 */
int DLB_DROM_PostFinalize(int pid, int return_stolen);

/* \brief Recover previously stolen CPUs if they are idle
 * \param[in] pid Process ID
 * \return 0 on success, -1 otherwise
 */
int DLB_DROM_RecoverStolenCpus(int pid);

#ifdef __cplusplus
}
#endif

#endif /* DLB_DROM_H */
