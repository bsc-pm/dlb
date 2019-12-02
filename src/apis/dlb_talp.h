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

#ifndef DLB_API_TALP_H
#define DLB_API_TALP_H

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************************/
/*    TALP                                                                       */
/*********************************************************************************/

/*! \brief Attach current process to DLB system as TALP
 * \return DLB_SUCCESS on success
 * */
int DLB_TALP_Attach(void);

/*! \brief Detach current process from DLB system
 * \return DLB_SUCCESS on success
 * */
int DLB_TALP_Detach(void);

/*! \brief Get the number of CPUs
 * \return DLB_SUCCESS on success
 * */
int DLB_TALP_GetNumCPUs(int *ncpus);

/*! \brief Get MPI Time of the current process
 * \return DLB_SUCCESS on success
 * */
int DLB_TALP_MPITimeGet(int process, double* mpi_time);

/*! \brief Get Compute Time of the current process
 * \return DLB_SUCCESS on success
 * */
int DLB_TALP_CPUTimeGet(int process, double* compute_time);

#ifdef __cplusplus
}
#endif


#endif /* DLB_API_TALP_H */
