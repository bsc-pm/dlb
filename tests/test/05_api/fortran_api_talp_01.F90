!-------------------------------------------------------------------------------!
!  Copyright 2009-2021 Barcelona Supercomputing Center                          !
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

! This test is only executed with meson because it requires 'talp_check_layout.c'
! which is only compiled for meson tests.
! For this reason, it explicitly lacks a test_generator for BETS.


!!!!! Notes about this test (Forgive me, world, for I have sinned) !!!!!
! Since Fortran's preprocessor capabilities are somewhat limited and do not
! fully support multi-line macros, this source file uses a technique in which
! the same file is included multiple times to "insert" code blocks via the
! FUNCTION_INTERFACE and CHECK_OFFSETS macros with different inputs.
! Maybe we should use Python to generate the source file, but let's keep it as
! is for now.


#define THIS_FILE "fortran_api_talp_01.F90"

#if defined(FUNCTION_INTERFACE)
        function offset_/**/STRUCT_NAME/**/_/**/FIELD() bind(c)
            import :: c_size_t
            integer(c_size_t) :: offset_/**/STRUCT_NAME/**/_/**/FIELD
        end function
#undef FIELD

#elif defined(CHECK_OFFSETS)

    c_offset_/**/FIELD = offset_/**/STRUCT_NAME/**/_/**/FIELD()
    loc_/**/FIELD = transfer(c_loc(STRUCT_VAR % FIELD), loc_/**/FIELD) - transfer(base, loc_/**/FIELD)
    print *, "Offset FIELD: C = ", c_offset_/**/FIELD, ", Fortran = ", loc_/**/FIELD
    if (c_offset_/**/FIELD /= loc_/**/FIELD) error stop "Mismatch: FIELD offset"
#undef FIELD

#else

module talp_check_layout
    use iso_c_binding
    implicit none

    include 'dlbf_talp.h'

    interface
        function sizeof_dlb_monitor_t() bind(c)
            import :: c_size_t
            integer(c_size_t) :: sizeof_dlb_monitor_t
        end function

#define FUNCTION_INTERFACE
#define STRUCT_NAME dlb_monitor_t
#define FIELD num_cpus
#include THIS_FILE
#define FIELD avg_cpus
#include THIS_FILE
#define FIELD cycles
#include THIS_FILE
#define FIELD instructions
#include THIS_FILE
#define FIELD num_measurements
#include THIS_FILE
#define FIELD num_resets
#include THIS_FILE
#define FIELD num_mpi_calls
#include THIS_FILE
#define FIELD num_omp_parallels
#include THIS_FILE
#define FIELD num_omp_tasks
#include THIS_FILE
#define FIELD num_gpu_runtime_calls
#include THIS_FILE
#define FIELD start_time
#include THIS_FILE
#define FIELD stop_time
#include THIS_FILE
#define FIELD elapsed_time
#include THIS_FILE
#define FIELD useful_time
#include THIS_FILE
#define FIELD mpi_time
#include THIS_FILE
#define FIELD omp_load_imbalance_time
#include THIS_FILE
#define FIELD omp_scheduling_time
#include THIS_FILE
#define FIELD omp_serialization_time
#include THIS_FILE
#define FIELD gpu_runtime_time
#include THIS_FILE
#define FIELD gpu_useful_time
#include THIS_FILE
#define FIELD gpu_communication_time
#include THIS_FILE
#define FIELD gpu_inactive_time
#include THIS_FILE
#undef STRUCT_NAME
#undef FUNCTION_INTERFACE

        function sizeof_dlb_pop_metrics_t() bind(c)
            import :: c_size_t
            integer(c_size_t) :: sizeof_dlb_pop_metrics_t
        end function

#define FUNCTION_INTERFACE
#define STRUCT_NAME dlb_pop_metrics_t
#define FIELD name
#include THIS_FILE
#define FIELD num_cpus
#include THIS_FILE
#define FIELD num_mpi_ranks
#include THIS_FILE
#define FIELD num_nodes
#include THIS_FILE
#define FIELD avg_cpus
#include THIS_FILE
#define FIELD num_gpus
#include THIS_FILE
#define FIELD cycles
#include THIS_FILE
#define FIELD instructions
#include THIS_FILE
#define FIELD num_measurements
#include THIS_FILE
#define FIELD num_mpi_calls
#include THIS_FILE
#define FIELD num_omp_parallels
#include THIS_FILE
#define FIELD num_omp_tasks
#include THIS_FILE
#define FIELD num_gpu_runtime_calls
#include THIS_FILE
#define FIELD elapsed_time
#include THIS_FILE
#define FIELD useful_time
#include THIS_FILE
#define FIELD mpi_time
#include THIS_FILE
#define FIELD omp_load_imbalance_time
#include THIS_FILE
#define FIELD omp_scheduling_time
#include THIS_FILE
#define FIELD omp_serialization_time
#include THIS_FILE
#define FIELD gpu_runtime_time
#include THIS_FILE
#define FIELD min_mpi_normd_proc
#include THIS_FILE
#define FIELD min_mpi_normd_node
#include THIS_FILE
#define FIELD gpu_useful_time
#include THIS_FILE
#define FIELD gpu_communication_time
#include THIS_FILE
#define FIELD gpu_inactive_time
#include THIS_FILE
#define FIELD max_gpu_useful_time
#include THIS_FILE
#define FIELD max_gpu_active_time
#include THIS_FILE
#define FIELD parallel_efficiency
#include THIS_FILE
#define FIELD mpi_parallel_efficiency
#include THIS_FILE
#define FIELD mpi_communication_efficiency
#include THIS_FILE
#define FIELD mpi_load_balance
#include THIS_FILE
#define FIELD mpi_load_balance_in
#include THIS_FILE
#define FIELD mpi_load_balance_out
#include THIS_FILE
#define FIELD omp_parallel_efficiency
#include THIS_FILE
#define FIELD omp_load_balance
#include THIS_FILE
#define FIELD omp_scheduling_efficiency
#include THIS_FILE
#define FIELD omp_serialization_efficiency
#include THIS_FILE
#define FIELD device_offload_efficiency
#include THIS_FILE
#define FIELD gpu_parallel_efficiency
#include THIS_FILE
#define FIELD gpu_load_balance
#include THIS_FILE
#define FIELD gpu_communication_efficiency
#include THIS_FILE
#define FIELD gpu_orchestration_efficiency
#include THIS_FILE
#undef STRUCT_NAME
#undef FUNCTION_INTERFACE

        function sizeof_dlb_node_metrics_t() bind(c)
            import :: c_size_t
            integer(c_size_t) :: sizeof_dlb_node_metrics_t
        end function

#define FUNCTION_INTERFACE
#define STRUCT_NAME dlb_node_metrics_t
#define FIELD name
#include THIS_FILE
#define FIELD node_id
#include THIS_FILE
#define FIELD processes_per_node
#include THIS_FILE
#define FIELD total_useful_time
#include THIS_FILE
#define FIELD total_mpi_time
#include THIS_FILE
#define FIELD max_useful_time
#include THIS_FILE
#define FIELD max_mpi_time
#include THIS_FILE
#define FIELD parallel_efficiency
#include THIS_FILE
#define FIELD communication_efficiency
#include THIS_FILE
#define FIELD load_balance
#include THIS_FILE
#undef STRUCT_NAME
#undef FUNCTION_INTERFACE

    end interface

end module talp_check_layout

program check_layout
    implicit none

    call check_dlb_monitor_layout()
    call check_dlb_pop_metrics_layout()
    call check_dlb_node_metrics_layout()

end program check_layout

subroutine check_dlb_monitor_layout
    use iso_c_binding
    use talp_check_layout
    implicit none

    type(dlb_monitor_t), target :: monitor
    integer(c_size_t) :: monitor_size
    type(c_ptr) :: base

    integer(c_size_t) :: c_offset_num_cpus,                loc_num_cpus
    integer(c_size_t) :: c_offset_avg_cpus,                loc_avg_cpus
    integer(c_size_t) :: c_offset_cycles,                  loc_cycles
    integer(c_size_t) :: c_offset_instructions,            loc_instructions
    integer(c_size_t) :: c_offset_num_measurements,        loc_num_measurements
    integer(c_size_t) :: c_offset_num_resets,              loc_num_resets
    integer(c_size_t) :: c_offset_num_mpi_calls,           loc_num_mpi_calls
    integer(c_size_t) :: c_offset_num_omp_parallels,       loc_num_omp_parallels
    integer(c_size_t) :: c_offset_num_omp_tasks,           loc_num_omp_tasks
    integer(c_size_t) :: c_offset_num_gpu_runtime_calls,   loc_num_gpu_runtime_calls
    integer(c_size_t) :: c_offset_start_time,              loc_start_time
    integer(c_size_t) :: c_offset_stop_time,               loc_stop_time
    integer(c_size_t) :: c_offset_elapsed_time,            loc_elapsed_time
    integer(c_size_t) :: c_offset_useful_time,             loc_useful_time
    integer(c_size_t) :: c_offset_mpi_time,                loc_mpi_time
    integer(c_size_t) :: c_offset_omp_load_imbalance_time, loc_omp_load_imbalance_time
    integer(c_size_t) :: c_offset_omp_scheduling_time,     loc_omp_scheduling_time
    integer(c_size_t) :: c_offset_omp_serialization_time,  loc_omp_serialization_time
    integer(c_size_t) :: c_offset_gpu_runtime_time,        loc_gpu_runtime_time
    integer(c_size_t) :: c_offset_gpu_useful_time,         loc_gpu_useful_time
    integer(c_size_t) :: c_offset_gpu_communication_time,  loc_gpu_communication_time
    integer(c_size_t) :: c_offset_gpu_inactive_time,       loc_gpu_inactive_time

    ! struct size
    monitor_size = sizeof_dlb_monitor_t()
    print *, 'dlb_monitor_t'
    print *, "Size (C)       :", monitor_size
    print *, "Size (Fortran) :", c_sizeof(monitor)
    if (monitor_size /= c_sizeof(monitor)) error stop "Mismatch: sizeof dlb_monitor_t"

    ! needed for offset checks
    base = c_loc(monitor)

#define CHECK_OFFSETS
#define STRUCT_NAME dlb_monitor_t
#define STRUCT_VAR monitor
#define FIELD num_cpus
#include THIS_FILE
#define FIELD avg_cpus
#include THIS_FILE
#define FIELD cycles
#include THIS_FILE
#define FIELD instructions
#include THIS_FILE
#define FIELD num_measurements
#include THIS_FILE
#define FIELD num_resets
#include THIS_FILE
#define FIELD num_mpi_calls
#include THIS_FILE
#define FIELD num_omp_parallels
#include THIS_FILE
#define FIELD num_omp_tasks
#include THIS_FILE
#define FIELD num_gpu_runtime_calls
#include THIS_FILE
#define FIELD start_time
#include THIS_FILE
#define FIELD stop_time
#include THIS_FILE
#define FIELD elapsed_time
#include THIS_FILE
#define FIELD useful_time
#include THIS_FILE
#define FIELD mpi_time
#include THIS_FILE
#define FIELD omp_load_imbalance_time
#include THIS_FILE
#define FIELD omp_scheduling_time
#include THIS_FILE
#define FIELD omp_serialization_time
#include THIS_FILE
#define FIELD gpu_runtime_time
#include THIS_FILE
#define FIELD gpu_useful_time
#include THIS_FILE
#define FIELD gpu_communication_time
#include THIS_FILE
#define FIELD gpu_inactive_time
#include THIS_FILE
#undef STRUCT_NAME
#undef STRUCT_VAR
#undef CHECK_OFFSETS

end subroutine check_dlb_monitor_layout

subroutine check_dlb_pop_metrics_layout
    use iso_c_binding
    use talp_check_layout
    implicit none

    type(dlb_pop_metrics_t), target :: pop_metrics
    integer(c_size_t) :: pop_metrics_size
    type(c_ptr) :: base

    integer(c_size_t) :: c_offset_name,                         loc_name
    integer(c_size_t) :: c_offset_num_cpus,                     loc_num_cpus
    integer(c_size_t) :: c_offset_num_mpi_ranks,                loc_num_mpi_ranks
    integer(c_size_t) :: c_offset_num_nodes,                    loc_num_nodes
    integer(c_size_t) :: c_offset_avg_cpus,                     loc_avg_cpus
    integer(c_size_t) :: c_offset_num_gpus,                     loc_num_gpus
    integer(c_size_t) :: c_offset_cycles,                       loc_cycles
    integer(c_size_t) :: c_offset_instructions,                 loc_instructions
    integer(c_size_t) :: c_offset_num_measurements,             loc_num_measurements
    integer(c_size_t) :: c_offset_num_mpi_calls,                loc_num_mpi_calls
    integer(c_size_t) :: c_offset_num_omp_parallels,            loc_num_omp_parallels
    integer(c_size_t) :: c_offset_num_omp_tasks,                loc_num_omp_tasks
    integer(c_size_t) :: c_offset_num_gpu_runtime_calls,        loc_num_gpu_runtime_calls
    integer(c_size_t) :: c_offset_elapsed_time,                 loc_elapsed_time
    integer(c_size_t) :: c_offset_useful_time,                  loc_useful_time
    integer(c_size_t) :: c_offset_mpi_time,                     loc_mpi_time
    integer(c_size_t) :: c_offset_omp_load_imbalance_time,      loc_omp_load_imbalance_time
    integer(c_size_t) :: c_offset_omp_scheduling_time,          loc_omp_scheduling_time
    integer(c_size_t) :: c_offset_omp_serialization_time,       loc_omp_serialization_time
    integer(c_size_t) :: c_offset_gpu_runtime_time,             loc_gpu_runtime_time
    integer(c_size_t) :: c_offset_min_mpi_normd_proc,           loc_min_mpi_normd_proc
    integer(c_size_t) :: c_offset_min_mpi_normd_node,           loc_min_mpi_normd_node
    integer(c_size_t) :: c_offset_gpu_useful_time,              loc_gpu_useful_time
    integer(c_size_t) :: c_offset_gpu_communication_time,       loc_gpu_communication_time
    integer(c_size_t) :: c_offset_gpu_inactive_time,            loc_gpu_inactive_time
    integer(c_size_t) :: c_offset_max_gpu_useful_time,          loc_max_gpu_useful_time
    integer(c_size_t) :: c_offset_max_gpu_active_time,          loc_max_gpu_active_time
    integer(c_size_t) :: c_offset_parallel_efficiency,          loc_parallel_efficiency
    integer(c_size_t) :: c_offset_mpi_parallel_efficiency,      loc_mpi_parallel_efficiency
    integer(c_size_t) :: c_offset_mpi_communication_efficiency, loc_mpi_communication_efficiency
    integer(c_size_t) :: c_offset_mpi_load_balance,             loc_mpi_load_balance
    integer(c_size_t) :: c_offset_mpi_load_balance_in,          loc_mpi_load_balance_in
    integer(c_size_t) :: c_offset_mpi_load_balance_out,         loc_mpi_load_balance_out
    integer(c_size_t) :: c_offset_omp_parallel_efficiency,      loc_omp_parallel_efficiency
    integer(c_size_t) :: c_offset_omp_load_balance,             loc_omp_load_balance
    integer(c_size_t) :: c_offset_omp_scheduling_efficiency,    loc_omp_scheduling_efficiency
    integer(c_size_t) :: c_offset_omp_serialization_efficiency, loc_omp_serialization_efficiency
    integer(c_size_t) :: c_offset_device_offload_efficiency,    loc_device_offload_efficiency
    integer(c_size_t) :: c_offset_gpu_parallel_efficiency,      loc_gpu_parallel_efficiency
    integer(c_size_t) :: c_offset_gpu_load_balance,             loc_gpu_load_balance
    integer(c_size_t) :: c_offset_gpu_communication_efficiency, loc_gpu_communication_efficiency
    integer(c_size_t) :: c_offset_gpu_orchestration_efficiency, loc_gpu_orchestration_efficiency

    ! struct size
    pop_metrics_size = sizeof_dlb_pop_metrics_t()
    print *, 'dlb_pop_metrics_t'
    print *, "Size (C)       :", pop_metrics_size
    print *, "Size (Fortran) :", c_sizeof(pop_metrics)
    if (pop_metrics_size /= c_sizeof(pop_metrics)) error stop "Mismatch: sizeof dlb_pop_metrics_t"

    ! needed for offset checks
    base = c_loc(pop_metrics)

#define CHECK_OFFSETS
#define STRUCT_NAME dlb_pop_metrics_t
#define STRUCT_VAR pop_metrics
#define FIELD name
#include THIS_FILE
#define FIELD num_cpus
#include THIS_FILE
#define FIELD num_mpi_ranks
#include THIS_FILE
#define FIELD num_nodes
#include THIS_FILE
#define FIELD avg_cpus
#include THIS_FILE
#define FIELD num_gpus
#include THIS_FILE
#define FIELD cycles
#include THIS_FILE
#define FIELD instructions
#include THIS_FILE
#define FIELD num_measurements
#include THIS_FILE
#define FIELD num_mpi_calls
#include THIS_FILE
#define FIELD num_omp_parallels
#include THIS_FILE
#define FIELD num_omp_tasks
#include THIS_FILE
#define FIELD num_gpu_runtime_calls
#include THIS_FILE
#define FIELD elapsed_time
#include THIS_FILE
#define FIELD useful_time
#include THIS_FILE
#define FIELD mpi_time
#include THIS_FILE
#define FIELD omp_load_imbalance_time
#include THIS_FILE
#define FIELD omp_scheduling_time
#include THIS_FILE
#define FIELD omp_serialization_time
#include THIS_FILE
#define FIELD gpu_runtime_time
#include THIS_FILE
#define FIELD min_mpi_normd_proc
#include THIS_FILE
#define FIELD min_mpi_normd_node
#include THIS_FILE
#define FIELD gpu_useful_time
#include THIS_FILE
#define FIELD gpu_communication_time
#include THIS_FILE
#define FIELD gpu_inactive_time
#include THIS_FILE
#define FIELD max_gpu_useful_time
#include THIS_FILE
#define FIELD max_gpu_active_time
#include THIS_FILE
#define FIELD parallel_efficiency
#include THIS_FILE
#define FIELD mpi_parallel_efficiency
#include THIS_FILE
#define FIELD mpi_communication_efficiency
#include THIS_FILE
#define FIELD mpi_load_balance
#include THIS_FILE
#define FIELD mpi_load_balance_in
#include THIS_FILE
#define FIELD mpi_load_balance_out
#include THIS_FILE
#define FIELD omp_parallel_efficiency
#include THIS_FILE
#define FIELD omp_load_balance
#include THIS_FILE
#define FIELD omp_scheduling_efficiency
#include THIS_FILE
#define FIELD omp_serialization_efficiency
#include THIS_FILE
#define FIELD device_offload_efficiency
#include THIS_FILE
#define FIELD gpu_parallel_efficiency
#include THIS_FILE
#define FIELD gpu_load_balance
#include THIS_FILE
#define FIELD gpu_communication_efficiency
#include THIS_FILE
#define FIELD gpu_orchestration_efficiency
#include THIS_FILE
#undef STRUCT_NAME
#undef STRUCT_VAR
#undef CHECK_OFFSETS

end subroutine check_dlb_pop_metrics_layout

subroutine check_dlb_node_metrics_layout
    use iso_c_binding
    use talp_check_layout
    implicit none

    type(dlb_node_metrics_t), target :: node_metrics
    integer(c_size_t) :: node_metrics_size
    type(c_ptr) :: base

    integer(c_size_t) :: c_offset_name,                     loc_name
    integer(c_size_t) :: c_offset_node_id,                  loc_node_id
    integer(c_size_t) :: c_offset_processes_per_node,       loc_processes_per_node
    integer(c_size_t) :: c_offset_total_useful_time,        loc_total_useful_time
    integer(c_size_t) :: c_offset_total_mpi_time,           loc_total_mpi_time
    integer(c_size_t) :: c_offset_max_useful_time,          loc_max_useful_time
    integer(c_size_t) :: c_offset_max_mpi_time,             loc_max_mpi_time
    integer(c_size_t) :: c_offset_parallel_efficiency,      loc_parallel_efficiency
    integer(c_size_t) :: c_offset_communication_efficiency, loc_communication_efficiency
    integer(c_size_t) :: c_offset_load_balance,             loc_load_balance

    ! struct size
    node_metrics_size = sizeof_dlb_node_metrics_t()
    print *, 'dlb_node_metrics_t'
    print *, "Size (C)       :", node_metrics_size
    print *, "Size (Fortran) :", c_sizeof(node_metrics)
    if (node_metrics_size /= c_sizeof(node_metrics)) error stop "Mismatch: sizeof dlb_node_metrics_t"

    ! needed for offset checks
    base = c_loc(node_metrics)

#define CHECK_OFFSETS
#define STRUCT_NAME dlb_node_metrics_t
#define STRUCT_VAR node_metrics
#define FIELD name
#include THIS_FILE
#define FIELD node_id
#include THIS_FILE
#define FIELD processes_per_node
#include THIS_FILE
#define FIELD total_useful_time
#include THIS_FILE
#define FIELD total_mpi_time
#include THIS_FILE
#define FIELD max_useful_time
#include THIS_FILE
#define FIELD max_mpi_time
#include THIS_FILE
#define FIELD parallel_efficiency
#include THIS_FILE
#define FIELD communication_efficiency
#include THIS_FILE
#define FIELD load_balance
#include THIS_FILE
#undef STRUCT_NAME
#undef STRUCT_VAR
#undef CHECK_OFFSETS

end subroutine check_dlb_node_metrics_layout

#endif
