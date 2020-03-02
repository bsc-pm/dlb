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

#include <time.h>
#include <stdint.h>

/* TALP structure */
typedef struct dlb_monitor_t {
    const char  *name;
    int         num_measurements;
    int         num_resets;
    int64_t     start_time;
    int64_t     end_time;
    int64_t     elapsed_time;
    int64_t     accumulated_MPI_time;
    int64_t     accumulated_computation_time;
    void        *_data;
} dlb_monitor_t;

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


/*********************************************************************************/
/*    TALP Monitoring Regions                                                    */
/*********************************************************************************/

/*! \brief Register a new Monitoring Region
 *  \param[in] name Name to identify the new region
 *  \return monitor handle to be used on subsequent calls
 */
dlb_monitor_t* DLB_MonitoringRegionRegister(const char *name);

/*! \brief Reset monitoring region
 *  \param[in] handle Monitoring handle that identifies the region
 *  \return DLB_SUCCESS on success
 */
int DLB_MonitoringRegionReset(dlb_monitor_t *handle);

/*! \brief Start (or unpause) monitoring region
 *  \param[in] handle Monitoring handle that identifies the region
 *  \return DLB_SUCCESS on success
 */
int DLB_MonitoringRegionStart(dlb_monitor_t *handle);

/*! \brief Stop (or pause) monitoring region
 *  \param[in] handle Monitoring handle that identifies the region
 *  \return DLB_SUCCESS on success
 */
int DLB_MonitoringRegionStop(dlb_monitor_t *handle);

/*! \brief Print a Report by stdout of the monitoring region
 *  \param[in] handle Monitoring handle that identifies the region
 *  \return DLB_SUCCESS on success
 */
int DLB_MonitoringRegionReport(dlb_monitor_t *handle);

#ifdef __cplusplus
}
#endif


#endif /* DLB_API_TALP_H */
