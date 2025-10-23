/*********************************************************************************/
/*  Copyright 2009-2025 Barcelona Supercomputing Center                          */
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

#define DLB_GLOBAL_REGION_NAME "Global"
#define DLB_GLOBAL_REGION NULL
#define DLB_MPI_REGION NULL         /* deprecated in favor of DLB_GLOBAL_REGION */
#define DLB_IMPLICIT_REGION NULL    /* deprecated in favor of DLB_GLOBAL_REGION */
#define DLB_LAST_OPEN_REGION (void*)1

enum { DLB_MONITOR_NAME_MAX = 128 };

/*! TALP public structure */
typedef struct dlb_monitor_t {
    /*! Name of the monitor */
    const char  *name;
    /*! Number of active CPUs */
    int         num_cpus;
    /*! Average of CPUs assigned to the process during the region execution */
    float       avg_cpus;
    /*! Number of measured cycles*/
    int64_t     cycles;
    /*! Number of measured instructions */
    int64_t     instructions;
    /*! Number of times that the region has been started and stopped */
    int         num_measurements;
    /*! Number of times that the region has been reset */
    int         num_resets;
    /*! Number of measured MPI calls */
    int64_t     num_mpi_calls;
    /*! Number of measured OpenMP parallel regions */
    int64_t     num_omp_parallels;
    /*! Number of measured OpenMP explicit tasks */
    int64_t     num_omp_tasks;
    /*! Number of measured GPU Runtime calls */
    int64_t     num_gpu_runtime_calls;
    /*! Absolute time (in nanoseconds) of the last time the region was started */
    int64_t     start_time;
    /*! Absolute time (in nanoseconds) of the last time the region was stopped */
    int64_t     stop_time;
    /*! Time (in nanoseconds) of the accumulated elapsed time inside the region */
    int64_t     elapsed_time;
    /*! Time (in nanoseconds) of the accumulated CPU time of useful computation inside the region */
    int64_t     useful_time;
    /*! Time (in nanoseconds) of the accumulated CPU time (not useful) in MPI inside the region */
    int64_t     mpi_time;
    /*! Time (in nanoseconds) of the accumulated CPU time (not useful) spent due to load
     * imbalance in OpenMP parallel regions */
    int64_t     omp_load_imbalance_time;
    /*! Time (in nanoseconds) of the accumulated CPU time (not useful) spent inside OpenMP
     * parallel regions due to scheduling and overhead, not counting load imbalance */
    int64_t     omp_scheduling_time;
    /*! Time (in nanoseconds) of the accumulated CPU time (not useful) spent outside
     * OpenMP parallel regions */
    int64_t     omp_serialization_time;
    /*! Time (in nanoseconds) of the accumulated CPU time (not useful) in GPU calls inside
     * the region */
    int64_t     gpu_runtime_time;
    /*! TBD */
    int64_t     gpu_useful_time;
    /*! TBD */
    int64_t     gpu_communication_time;
    /*! TBD */
    int64_t     gpu_inactive_time;
    /*! Internal data */
    void        *_data;
} dlb_monitor_t;

/*! POP metrics (of one monitor) collected among all processes */
typedef struct dlb_pop_metrics_t {
    /*! Name of the monitor */
    char        name[DLB_MONITOR_NAME_MAX];
    /*! Total number of CPUs used by the processes that have used the region */
    int         num_cpus;
    /*! Total number of mpi processes that have used the region */
    int         num_mpi_ranks;
    /*! Total number of nodes used by the processes that have used the region */
    int         num_nodes;
    /*! Total average of CPUs used in the region. Only meaningful if LeWI enabled. */
    float       avg_cpus;
    /*! TBD */
    int         num_gpus;
    /*! Total number of CPU cycles elapsed in that region during useful time */
    double      cycles;
    /*! Total number of instructions executed during useful time */
    double      instructions;
    /*! Number of times that the region has been started and stopped among all processes */
    int64_t     num_measurements;
    /*! Number of executed MPI calls combined among all MPI processes */
    int64_t     num_mpi_calls;
    /*! Number of encountered OpenMP parallel regions combined among all processes */
    int64_t     num_omp_parallels;
    /*! Number of encountered OpenMP tasks combined among all processes */
    int64_t     num_omp_tasks;
    /*! Number of executed GPU Runtime calls combined among all processes */
    int64_t     num_gpu_runtime_calls;
    /*! Time (in nanoseconds) of the accumulated elapsed time inside the region */
    int64_t     elapsed_time;
    /*! Time (in nanoseconds) of the accumulated CPU time of useful computation in the application */
    int64_t     useful_time;
    /*! Time (in nanoseconds) of the accumulated CPU time (not useful) in MPI */
    int64_t     mpi_time;
    /*! Time (in nanoseconds) of the accumulated CPU time (not useful) spent due to load
     * imbalance in OpenMP parallel regions */
    int64_t     omp_load_imbalance_time;
    /*! Time (in nanoseconds) of the accumulated CPU time (not useful) spent inside OpenMP
     * parallel regions due to scheduling and overhead, not counting load imbalance */
    int64_t     omp_scheduling_time;
    /*! Time (in nanoseconds) of the accumulated CPU time (not useful) spent outside
     * OpenMP parallel regions */
    int64_t     omp_serialization_time;
    /*! Time (in nanoseconds) of the accumulated CPU time (not useful) in GPU calls */
    int64_t     gpu_runtime_time;
    /*! MPI time normalized at process level of the process with less MPI */
    double      min_mpi_normd_proc;
    /*! MPI time normalized at node level of the node with less MPI */
    double      min_mpi_normd_node;
    /* TBD */
    int64_t     gpu_useful_time;
    /* TBD */
    int64_t     gpu_communication_time;
    /* TBD: remove? */
    int64_t     gpu_inactive_time;
    /* TBD */
    int64_t     max_gpu_useful_time;
    /* TBD */
    int64_t     max_gpu_active_time;
    /*! Efficiency number [0.0 - 1.0] of the impact in the application's parallelization */
    float       parallel_efficiency;
    /*! Efficiency number of the impact in the MPI parallelization */
    float       mpi_parallel_efficiency;
    /*! Efficiency lost due to MPI transfer and serialization */
    float       mpi_communication_efficiency;
    /*! Efficiency of the MPI Load Balance */
    float       mpi_load_balance;
    /*! Intra-node MPI Load Balance coefficient */
    float       mpi_load_balance_in;
    /*! Inter-node MPI Load Balance coefficient */
    float       mpi_load_balance_out;
    /*! Efficiency number of the impact in the OpenMP parallelization */
    float       omp_parallel_efficiency;
    /*! Efficiency of the OpenMP Load Balance inside parallel regions */
    float       omp_load_balance;
    /*! Efficiency of the OpenMP scheduling inside parallel regions */
    float       omp_scheduling_efficiency;
    /*! Efficiency lost due to OpenMP threads outside of parallel regions */
    float       omp_serialization_efficiency;
    /*! Efficiency of the Host offloading to the Device */
    float       device_offload_efficiency;
    /*! TBD */
    float       gpu_parallel_efficiency;
    /*! TBD */
    float       gpu_load_balance;
    /*! TBD */
    float       gpu_communication_efficiency;
    /*! TBD */
    float       gpu_orchestration_efficiency;
} dlb_pop_metrics_t;

/*! Node metrics (of one monitor) collected asynchronously from the shared memory */
typedef struct dlb_node_metrics_t {
    /*! Name of the monitor */
    char    name[DLB_MONITOR_NAME_MAX];
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

/*! Node raw times (of one monitor) collected asynchronously from the shared memory */
typedef struct dlb_node_times_t {
    /*! Process ID */
    pid_t pid;
    /*! Time (in nanoseconds) of the accumulated CPU time of communication */
    int64_t mpi_time;
    /*! Time (in nanoseconds) of the accumulated CPU time of useful computation */
    int64_t useful_time;
} dlb_node_times_t;

#ifdef __cplusplus
extern "C"
{
#endif

/*********************************************************************************/
/*                                                                               */
/*  The following functions are intended to be called from 1st-party or          */
/*  3rd-party programs indistinctly; that is, DLB applications, or external      */
/*  profilers as long as they invoke DLB_TALP_Attach.                            */
/*                                                                               */
/*********************************************************************************/

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
 *  internal DLB file descriptors and clean data.
 */
int DLB_TALP_Detach(void);

/*! \brief Get the number of CPUs in the node
 *  \param[out] ncpus the number of CPUs
 *  \return DLB_SUCCESS on success
 */
int DLB_TALP_GetNumCPUs(int *ncpus);

/*! \brief Get the list of running processes registered in the DLB system
 *  \param[out] pidlist The output list
 *  \param[out] nelems Number of elements in the list
 *  \param[in] max_len Max capacity of the list
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory
 */
int DLB_TALP_GetPidList(int *pidlist, int *nelems, int max_len);

/*! \brief Get the CPU time spent on MPI and useful computation for the given process
 *  \param[in] pid target Process ID, or 0 if own process
 *  \param[out] mpi_time CPU time spent on MPI in seconds
 *  \param[out] useful_time CPU time spend on useful computation in seconds
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOPROC if target pid is not registered in the DLB system
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory
 *  \return DLB_ERR_NOTALP if target is own process and TALP is not enabled
 */
int DLB_TALP_GetTimes(int pid, double *mpi_time, double *useful_time);

/*! \brief Get the list of raw times for the specified region
 *  \param[in] name Name to identify the region
 *  \param[out] node_times_list The output list
 *  \param[out] nelems Number of elements in the list
 *  \param[in] max_len Max capacity of the list
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory
 *
 *  Note: This function requires DLB_ARGS+=" --talp-external-profiler" even if
 *  it's called from 1st-party programs.
 */
int DLB_TALP_GetNodeTimes(const char *name, dlb_node_times_t *node_times_list,
        int *nelems, int max_len);

/*! \brief From either 1st or 3rd party, query node metrics for one region
 *  \param[in] name Name to identify the region
 *  \param[out] node_metrics Allocated structure where the collected metrics will be stored
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOENT if no data for the given name
 *  \return DLB_ERR_NOSHMEM if cannot find shared memory
 *
 *  Note: This function requires DLB_ARGS+=" --talp-external-profiler" even if
 *  it's called from 1st-party programs.
 */
int DLB_TALP_QueryPOPNodeMetrics(const char *name, dlb_node_metrics_t *node_metrics);


/*********************************************************************************/
/*                                                                               */
/*  The functions declared below are intended to be called only from 1st-party   */
/*  programs, and they should return an error if they are called from external   */
/*  profilers.                                                                   */
/*                                                                               */
/*  DISCLAIMER: This header file may be split in two in the next major release.  */
/*                                                                               */
/*********************************************************************************/

/*********************************************************************************/
/*    TALP Monitoring Regions                                                    */
/*********************************************************************************/

/*! \brief Get the pointer of the global application-wide Monitoring Region
 *  \return monitor handle to be used on queries, or NULL if TALP is not enabled
 */
dlb_monitor_t* DLB_MonitoringRegionGetGlobal(void);

dlb_monitor_t* DLB_MonitoringRegionGetImplicit(void)
    __attribute__((deprecated("DLB_MonitoringRegionGetGlobal")));

const dlb_monitor_t* DLB_MonitoringRegionGetMPIRegion(void)
    __attribute__((deprecated("DLB_MonitoringRegionGetGlobal")));

/*! \brief Register a new Monitoring Region, or obtain the associated pointer by name
 *  \param[in] name Name to identify the region
 *  \return monitor handle to be used on subsequent calls, or NULL if TALP is not enabled
 *
 *  This function registers a new monitoring region or obtains the pointer to
 *  an already created region with the same name. The name "Global" is a
 *  special reserved name (case-insensitive); invoking this function with this name is
 *  equivalent as invoking DLB_MonitoringRegionGetGlobal(). Otherwise, the region
 *  name is treated case-sensitive.
 */
dlb_monitor_t* DLB_MonitoringRegionRegister(const char *name);

/*! \brief Reset monitoring region
 *  \param[in] handle Monitoring handle that identifies the region, or DLB_GLOBAL_REGION
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOTALP if TALP is not enabled
 *
 *  Reset all values of the monitoring region except num_resets, which is incremented.
 *  If the region is open, discard all intermediate values and close it.
 */
int DLB_MonitoringRegionReset(dlb_monitor_t *handle);

/*! \brief Start (or unpause) monitoring region
 *  \param[in] handle Monitoring handle that identifies the region, or DLB_GLOBAL_REGION
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOTALP if TALP is not enabled
 *  \return DLB_ERR_PERM if this thread cannot start the monitoring region
 *
 *  Notes on multi-threading:
 *    - It is not safe to start or stop regions in OpenMP worksharing constructs.
 *    - If a region is started and stopped before the application has reached
 *      maximum parallelism (e.g., before a parallel construct), the unused resources
 *      will not be taken into account. This can result in higher OpenMP efficiencies
 *      than expected.
 */
int DLB_MonitoringRegionStart(dlb_monitor_t *handle);

/*! \brief Stop (or pause) monitoring region
 *  \param[in] handle Monitoring handle that identifies the region, DLB_GLOBAL_REGION,
 *                    or DLB_LAST_OPEN_REGION
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOTALP if TALP is not enabled
 *  \return DLB_ERR_NOENT if DLB_LAST_OPEN_REGION does not match any region
 *  \return DLB_ERR_PERM if this thread cannot stop the monitoring region
 */
int DLB_MonitoringRegionStop(dlb_monitor_t *handle);

/*! \brief Print a report to stderr of the monitoring region
 *  \param[in] handle Monitoring handle that identifies the region, or DLB_GLOBAL_REGION
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOTALP if TALP is not enabled
 */
int DLB_MonitoringRegionReport(const dlb_monitor_t *handle);

/*! \brief Update all monitoring regions
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_PERM if this thread cannot update the monitoring region
 *
 *  Monitoring regions are only updated in certain situations, like when
 *  starting/stopping a region, or finalizing MPI. This routine forces the
 *  update of all started monitoring regions
*/
int DLB_MonitoringRegionsUpdate(void);

/*! \brief Perform an MPI collective communication to collect POP metrics.
 *  \param[in] monitor Monitoring handle that identifies the region,
 *                     or DLB_GLOBAL_REGION macro (NULL) if global application-wide region
 *  \param[out] pop_metrics Allocated structure where the collected metrics will be stored
 *  \return DLB_SUCCESS on success
 *  \return DLB_ERR_NOTALP if TALP is not enabled
 */
int DLB_TALP_CollectPOPMetrics(dlb_monitor_t *monitor, dlb_pop_metrics_t *pop_metrics);

/*! \brief Perform a node collective communication to collect TALP node metrics.
 *  \param[in] monitor Monitoring handle that identifies the region,
 *                     or DLB_GLOBAL_REGION macro (NULL) if global application-wide region
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
