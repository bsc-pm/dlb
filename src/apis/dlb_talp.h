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

#ifndef DLB_API_TALP_H
#define DLB_API_TALP_H

#include <time.h>
#include <stdint.h>

#define DLB_MPI_REGION NULL

enum { DLB_MONITOR_NAME_MAX = 128 };

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
    /*! Number of measured MPI calls */
    int64_t     num_mpi_calls;
    /*! Number of measured instructions */
    int64_t     accumulated_instructions;
    /*! Number of measured cycles*/
    int64_t     accumulated_cycles;
    /*! Internal data */
    void        *_data;
} dlb_monitor_t;

/*! POP metrics (of one monitor) collected from all processes */
typedef struct dlb_pop_metrics_t {
    /*! Name of the monitor */
    char    name[DLB_MONITOR_NAME_MAX];
    /*! Total number of CPUs used by the processes that have used the region */
    int     total_cpus;
    /*! Total number of nodes used by the processes that have used the region */
    int     total_nodes;
    /*! Time (in nanoseconds) of the accumulated elapsed time inside the region */
    int64_t elapsed_time;
    /*! Time (in nanoseconds) of the accumulated elapsed time in computation inside the region */
    int64_t elapsed_useful;
    /*! Time (in nanoseconds) of the accumulated CPU time of useful computation in all the nodes */
    int64_t app_sum_useful;
    /*! Time (in nanoseconds) of the accumulated CPU time of useful computation in the most
     * loaded node*/
    int64_t node_sum_useful;
    /*! ParallelEfficiency = Communication Efficiency * LoadBalance */
    float   parallel_efficiency;
    /*! Efficiency lost due to transfer and serialization */
    float   communication_efficiency;
    /*! LoadBalance = LoadBalanceIn * LoadBalanceOut */
    float   lb;
    /*! Intra-node Load Balance coefficient */
    float   lb_in;
    /*! Inter-node Load Balance coefficient */
    float   lb_out;
} dlb_pop_metrics_t;

/*! Node metrics (of one monitor) collected asynchronously from the shared memory */
typedef struct dlb_node_metrics_t {
    /*! Node identifier, only meaningful value if MPI is enabled */
    int     node_id;
    /*! Number of processes per node */
    int     processes_per_node;
    /*! Time (in nanoseconds) of the accumulated CPU time of useful computation
     * of all processes in the node */
    int64_t total_useful_time;
    /*! Time (in nanoseconds) of the accumulated CPU time of communication
     * of all processes in the node */
    int64_t total_mpi_time;
    /*! Time (in nanoseconds) of the accumulated CPU time of useful computation
     * of the process with the highest value */
    int64_t max_useful_time;
    /*! Time (in nanoseconds) of the accumulated CPU time of communication
     * of the process with the highest value */
    int64_t max_mpi_time;
    /*! Node ParallelEfficiency = Communication Efficiency * LoadBalance */
    float   parallel_efficiency;
    /*! Node efficiency lost due to transfer and serialization */
    float   communication_efficiency;
    /*! Node Load Balance coefficient */
    float   load_balance;
} dlb_node_metrics_t;

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
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory to detach from
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

/*! \brief Get the pointer of the implicit MPI Monitoring Region
 *  \return monitor handle to be used on queries, or NULL if TALP is not enabled
 */
const dlb_monitor_t* DLB_MonitoringRegionGetMPIRegion(void);

/*! \brief Register a new Monitoring Region
 *  \param[in] name Name to identify the new region
 *  \return monitor handle to be used on subsequent calls, or NULL if TALP is not enabled
 */
dlb_monitor_t* DLB_MonitoringRegionRegister(const char *name);

/*! \brief Reset monitoring region
 *  \param[in] handle Monitoring handle that identifies the region, or DLB_MPI_REGION
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOTALP if TALP is not enabled
 */
int DLB_MonitoringRegionReset(dlb_monitor_t *handle);

/*! \brief Start (or unpause) monitoring region
 *  \param[in] handle Monitoring handle that identifies the region, or DLB_MPI_REGION
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOTALP if TALP is not enabled
 */
int DLB_MonitoringRegionStart(dlb_monitor_t *handle);

/*! \brief Stop (or pause) monitoring region
 *  \param[in] handle Monitoring handle that identifies the region, or DLB_MPI_REGION
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOTALP if TALP is not enabled
 */
int DLB_MonitoringRegionStop(dlb_monitor_t *handle);

/*! \brief Print a report to stderr of the monitoring region
 *  \param[in] handle Monitoring handle that identifies the region, or DLB_MPI_REGION
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOTALP if TALP is not enabled
 */
int DLB_MonitoringRegionReport(const dlb_monitor_t *handle);

 /*! \brief Update all monitoring regions
  *  \return DLB_SUCCESS on success
  *
  *  Monitoring regions are only updated in certain situations, like when
  *  starting/stopping a region, or finalizing MPI. This routine forces the
  *  update of all started monitoring regions
 */
int DLB_MonitoringRegionsUpdate(void);

/*! \brief Perform an MPI collective communication to collect POP metrics.
 *  \param[in] monitor Monitoring handle that identifies the region,
 *                     or DLB_MPI_REGION macro (NULL) if implicit MPI region
 *  \param[out] pop_metrics Allocated structure where the collected metrics will be stored
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOTALP if TALP is not enabled
 */
int DLB_TALP_CollectPOPMetrics(dlb_monitor_t *monitor, dlb_pop_metrics_t *pop_metrics);

/*! \brief Perform a node collective communication to collect TALP node metrics.
 *  \param[in] monitor Monitoring handle that identifies the region,
 *                     or DLB_MPI_REGION macro (NULL) if implicit MPI region
 *  \param[out] node_metrics Allocated structure where the collected metrics will be stored
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOTALP if TALP is not enabled
 *  \return DLB_ERR_NOCOMP if support for barrier is disabled, i.e., --no-barrier
 *
 *  This functions performs a node barrier to collect the data. All processes that
 *  are running in the node must invoke this function.
 */
int DLB_TALP_CollectPOPNodeMetrics(dlb_monitor_t *monitor, dlb_node_metrics_t *node_metrics);

int DLB_TALP_CollectNodeMetrics(dlb_monitor_t *monitor, dlb_node_metrics_t *node_metrics)
    __attribute__((deprecated("DLB_TALP_CollectPOPNodeMetrics")));

#ifdef __cplusplus
}
#endif


#endif /* DLB_API_TALP_H */
