!-------------------------------------------------------------------------------!
!  Copyright 2009-2025 Barcelona Supercomputing Center                          !
!                                                                               !
!  This file is part of the DLB library.                                        !
!                                                                               !
!  DLB is free software: you can redistribute it and/or modify                  !
!  it under the terms of the GNU Lesser General Public License as published by  !
!  the Free Software Foundation, either version 3 of the License, or            !
!  (at your option) any later version.                                          !
!                                                                               !
!  DLB is distributed in the hope that it will be useful,                       !
!  but WITHOUT ANY WARRANTY; without even the implied warranty of               !
!  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                !
!  GNU Lesser General Public License for more details.                          !
!                                                                               !
!  You should have received a copy of the GNU Lesser General Public License     !
!  along with DLB.  If not, see <https://www.gnu.org/licenses/>.                !
!-------------------------------------------------------------------------------!

       character(len=*), parameter :: DLB_GLOBAL_REGION_NAME = "Global"
       type(c_ptr), parameter   :: DLB_GLOBAL_REGION = c_null_ptr
       type(c_ptr), parameter   :: DLB_MPI_REGION = c_null_ptr      !! deprecated
       type(c_ptr), parameter   :: DLB_IMPLICIT_REGION = c_null_ptr !! deprecated
       integer(c_intptr_t), parameter :: DLB_GLOBAL_REGION_INT = 0
       integer(c_intptr_t), parameter :: DLB_LAST_OPEN_REGION_INT = 1
       integer, parameter       :: DLB_MONITOR_NAME_MAX = 128

       type, bind(c) :: dlb_monitor_t
        type(c_ptr)             :: name_
        integer(kind=c_int)     :: num_cpus
        real(kind=c_float)      :: avg_cpus
        integer(kind=c_int64_t) :: cycles
        integer(kind=c_int64_t) :: instructions
        integer(kind=c_int)     :: num_measurements
        integer(kind=c_int)     :: num_resets
        integer(kind=c_int64_t) :: num_mpi_calls
        integer(kind=c_int64_t) :: num_omp_parallels
        integer(kind=c_int64_t) :: num_omp_tasks
        integer(kind=c_int64_t) :: num_gpu_runtime_calls
        integer(kind=c_int64_t) :: start_time
        integer(kind=c_int64_t) :: stop_time
        integer(kind=c_int64_t) :: elapsed_time
        integer(kind=c_int64_t) :: useful_time
        integer(kind=c_int64_t) :: mpi_time
        integer(kind=c_int64_t) :: omp_load_imbalance_time
        integer(kind=c_int64_t) :: omp_scheduling_time
        integer(kind=c_int64_t) :: omp_serialization_time
        integer(kind=c_int64_t) :: gpu_runtime_time
        integer(kind=c_int64_t) :: gpu_useful_time
        integer(kind=c_int64_t) :: gpu_communication_time
        integer(kind=c_int64_t) :: gpu_inactive_time
        type(c_ptr)             :: data_
       end type

       type, bind(c) :: dlb_pop_metrics_t
        character(kind=c_char, len=1) :: name(DLB_MONITOR_NAME_MAX)
        integer(kind=c_int)     :: num_cpus
        integer(kind=c_int)     :: num_mpi_ranks
        integer(kind=c_int)     :: num_nodes
        real(kind=c_float)      :: avg_cpus
        integer(kind=c_int)     :: num_gpus
        real(kind=c_double)     :: cycles
        real(kind=c_double)     :: instructions
        integer(kind=c_int64_t) :: num_measurements
        integer(kind=c_int64_t) :: num_mpi_calls
        integer(kind=c_int64_t) :: num_omp_parallels
        integer(kind=c_int64_t) :: num_omp_tasks
        integer(kind=c_int64_t) :: num_gpu_runtime_calls
        integer(kind=c_int64_t) :: elapsed_time
        integer(kind=c_int64_t) :: useful_time
        integer(kind=c_int64_t) :: mpi_time
        integer(kind=c_int64_t) :: omp_load_imbalance_time
        integer(kind=c_int64_t) :: omp_scheduling_time
        integer(kind=c_int64_t) :: omp_serialization_time
        integer(kind=c_int64_t) :: gpu_runtime_time
        real(kind=c_double)     :: min_mpi_normd_proc
        real(kind=c_double)     :: min_mpi_normd_node
        integer(kind=c_int64_t) :: gpu_useful_time
        integer(kind=c_int64_t) :: gpu_communication_time
        integer(kind=c_int64_t) :: gpu_inactive_time
        integer(kind=c_int64_t) :: max_gpu_useful_time
        integer(kind=c_int64_t) :: max_gpu_active_time
        real(kind=c_float)      :: parallel_efficiency
        real(kind=c_float)      :: mpi_parallel_efficiency
        real(kind=c_float)      :: mpi_communication_efficiency
        real(kind=c_float)      :: mpi_load_balance
        real(kind=c_float)      :: mpi_load_balance_in
        real(kind=c_float)      :: mpi_load_balance_out
        real(kind=c_float)      :: omp_parallel_efficiency
        real(kind=c_float)      :: omp_load_balance
        real(kind=c_float)      :: omp_scheduling_efficiency
        real(kind=c_float)      :: omp_serialization_efficiency
        real(kind=c_float)      :: device_offload_efficiency
        real(kind=c_float)      :: gpu_parallel_efficiency
        real(kind=c_float)      :: gpu_load_balance
        real(kind=c_float)      :: gpu_communication_efficiency
        real(kind=c_float)      :: gpu_orchestration_efficiency
       end type

       type, bind(c) :: dlb_node_metrics_t
        character(kind=c_char, len=1) :: name(DLB_MONITOR_NAME_MAX)
        integer(kind=c_int)     :: node_id
        integer(kind=c_int)     :: processes_per_node
        integer(kind=c_int64_t) :: total_useful_time
        integer(kind=c_int64_t) :: total_mpi_time
        integer(kind=c_int64_t) :: max_useful_time
        integer(kind=c_int64_t) :: max_mpi_time
        real(kind=c_float)      :: parallel_efficiency
        real(kind=c_float)      :: communication_efficiency
        real(kind=c_float)      :: load_balance
       end type

       interface

        !---------------------------------------------------------------------------!
        ! The following functions are intended to be called from 1st-party or
        ! 3rd-party programs indistinctly; that is, DLB applications, or external
        ! profilers as long as they invoke DLB_TALP_Attach.
        !---------------------------------------------------------------------------!

        function dlb_talp_attach() result(ierr)                         &
     &          bind(c,name='DLB_TALP_Attach')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_talp_attach

        function dlb_talp_detach() result(ierr)                         &
     &          bind(c,name='DLB_TALP_Detach')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_talp_detach

        function dlb_talp_getnumcpus(ncpus) result(ierr)                &
     &          bind(c,name='DLB_TALP_GetNumCPUs')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            real(c_double), intent(out) :: ncpus
        end function dlb_talp_getnumcpus

        function dlb_talp_querypopnodemetrics(name, node_metrics)       &
                result(ierr)                                            &
                bind(c,name='DLB_TALP_QueryPOPNodeMetrics')
            use iso_c_binding
            import :: dlb_node_metrics_t
            integer(kind=c_int) :: ierr
            character(kind=c_char), intent(in) :: name(*)
            type(dlb_node_metrics_t), intent(out) :: node_metrics
        end function dlb_talp_querypopnodemetrics


        !---------------------------------------------------------------------------!
        ! The functions declared below are intended to be called only from
        ! 1st-party programs, and they should return an error if they are
        ! called from external profilers.
        !---------------------------------------------------------------------------!

        function dlb_monitoringregiongetglobal()                        &
     &          result (handle)                                         &
     &          bind(c, name='DLB_MonitoringRegionGetGlobal')
            use iso_c_binding
            type(c_ptr) :: handle
        end function dlb_monitoringregiongetglobal

        !! deprecated: bind to DLB_MonitoringRegionGetGlobal()
        function dlb_monitoringregiongetimplicit()                      &
     &          result (handle)                                         &
     &          bind(c, name='DLB_MonitoringRegionGetGlobal')
            use iso_c_binding
            type(c_ptr) :: handle
        end function dlb_monitoringregiongetimplicit

        function dlb_monitoringregionregister(region_name)              &
     &          result (handle)                                         &
     &          bind(c, name='DLB_MonitoringRegionRegister')
            use iso_c_binding
            type(c_ptr) :: handle
            character(kind=c_char), intent(in) :: region_name(*)
        end function dlb_monitoringregionregister

        function dlb_monitoringregionreset(handle)                      &
     &         result (ierr) bind(c, name='DLB_MonitoringRegionReset')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: handle
        end function dlb_monitoringregionreset

        function dlb_monitoringregionstart(handle)                      &
     &         result (ierr) bind(c, name='DLB_MonitoringRegionStart')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: handle
        end function dlb_monitoringregionstart

        function dlb_monitoringregionstop(handle)                       &
     &         result (ierr) bind(c, name='DLB_MonitoringRegionStop')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: handle
        end function dlb_monitoringregionstop

        function dlb_monitoringregionreport(handle)                     &
     &         result (ierr) bind(c, name='DLB_MonitoringRegionReport')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: handle
        end function dlb_monitoringregionreport

        function dlb_monitoringregionupdate()                           &
     &         result (ierr) bind(c, name='DLB_MonitoringRegionsUpdate')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_monitoringregionupdate

        function dlb_talp_collectpopmetrics(monitor, pop_metrics)      &
                result (ierr) bind(c, name='DLB_TALP_CollectPOPMetrics')
            use iso_c_binding
            import :: dlb_pop_metrics_t
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: monitor
            type(dlb_pop_metrics_t), intent(out) :: pop_metrics
        end function dlb_talp_collectpopmetrics

        function dlb_talp_collectpopnodemetrics(monitor, node_metrics)  &
                result (ierr) bind(c, name='DLB_TALP_CollectPOPNodeMetrics')
            use iso_c_binding
            import :: dlb_node_metrics_t
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: monitor
            type(dlb_node_metrics_t), intent(out) :: node_metrics
        end function dlb_talp_collectpopnodemetrics
      end interface

! -*- fortran -*-  vim: set ft=fortran:
