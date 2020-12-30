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

/*! TALP public structure */
typedef struct dlb_monitor_t {
    /*! Name of the monitor */
    const char  *name;
    /*! Number of times that the region has been started and stopped */
    int         num_measurements;
    /*! Number of times that the region has been reset */
    int         num_resets;
    /*! Absolute time (in nanoseconds) of the last time the region was started */
    int64_t     start_time;
    /*! Absolute time (in nanoseconds) of the last time the region was stopped */
    int64_t     stop_time;
    /*! Time (in nanoseconds) of the accumulated elapsed time inside the region */
    int64_t     elapsed_time;
    /*! Time (in nanoseconds) of the accumulated elapsed time in computation inside the region */
    int64_t     elapsed_computation_time;
    /*! Time (in nanoseconds) of the accumulated CPU time in MPI inside the region */
    int64_t     accumulated_MPI_time;
    /*! Time (in nanoseconds) of the accumulated CPU time in computation inside the region */
    int64_t     accumulated_computation_time;
    /*! Internal data */
    void        *_data;
} dlb_monitor_t;

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************************/
/*    TALP                                                                       */
/*********************************************************************************/

/*! \brief Attach current process to DLB system as TALP administrator
 *  \return DLB_SUCCESS on success
 *
 *  Once the process is attached to DLB as TALP administrator, it may perform the
 *  below actions described in this file. This way, the process is able to obtain
 *  some TALP values such as time spent in computation or MPI for each of the DLB
 *  running processes.
 */
int DLB_TALP_Attach(void);

/*! \brief Detach current process from DLB system
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory to dettach from
 *
 *  If previously attached, a process must call this function to correctly close
 *  file descriptors and clean data.
 */
int DLB_TALP_Detach(void);

/*! \brief Get the number of CPUs in the node
 *  \param[out] ncpus the number of CPUs
 *  \return DLB_SUCCESS on success
 */
int DLB_TALP_GetNumCpus(int *ncpus);

/*! \brief Get the list of running processes registered in the DLB system
 *  \param[out] pidlist The output list
 *  \param[out] nelems Number of elements in the list
 *  \param[in] max_len Max capacity of the list
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory
 */
int DLB_TALP_GetPidList(int *pidlist, int *nelems, int max_len);

/*! \brief Get the CPU time spent on MPI and useful computation for the given process
 *  \param[in] pid target Process ID
 *  \param[out] mpi_time CPU time spent on MPI in seconds
 *  \param[out] useful_time CPU time spend on useful computation in seconds
 *  \return DLB_SUCCESS on success
 */
int DLB_TALP_GetTimes(int pid, double *mpi_time, double *useful_time);


/*********************************************************************************/
/*    TALP Monitoring Regions                                                    */
/*********************************************************************************/

/*! \brief Get the pointer of the implicit MPI Monitorig Region
 *  \return monitor handle to be used on queries
 */
const dlb_monitor_t* DLB_MonitoringRegionGetMPIRegion(void);

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

/*! \brief Print a report to stderr of the monitoring region
 *  \param[in] handle Monitoring handle that identifies the region
 *  \return DLB_SUCCESS on success
 */
int DLB_MonitoringRegionReport(const dlb_monitor_t *handle);

#ifdef __cplusplus
}
#endif


#endif /* DLB_API_TALP_H */
