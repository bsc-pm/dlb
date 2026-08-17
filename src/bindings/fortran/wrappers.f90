!-------------------------------------------------------------------------------!
!  Copyright 2009-2026 Barcelona Supercomputing Center                          !
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


function dlb_init(ncpus, mask, dlb_args) result (ierr)
    use :: iso_c_binding
    use :: mod_string, only: string_f2c
    implicit none
    integer(kind=c_int) :: ierr
    integer(kind=c_int), value, intent(in) :: ncpus
    type(c_ptr), value, intent(in) :: mask
    character(len=*), intent(in) :: dlb_args

    character(kind=c_char) :: dlb_args_c(len_trim(dlb_args)+1)

    interface
        function dlb_init_c(ncpus, mask, dlb_args) result (ierr)          &
     &          bind(c, name='DLB_Init')
            use :: iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: ncpus
            type(c_ptr), value, intent(in) :: mask
            character(kind=c_char), intent(in) :: dlb_args(*)
        end function dlb_init_c
    end interface

    call string_f2c(dlb_args, dlb_args_c)

    ierr = dlb_init_c(ncpus, mask, dlb_args_c)

end function dlb_init


function dlb_barriernamedregister(barrier_name, flags) result (handle)
    use :: iso_c_binding
    use :: mod_string, only: string_f2c
    implicit none
    type(c_ptr) :: handle
    character(len=*), intent(in) :: barrier_name
    integer(kind=c_int), value, intent(in) :: flags

    character(kind=c_char) :: barrier_name_c(len_trim(barrier_name)+1)

    interface
        function dlb_barriernamedregister_c(barrier_name, flags)        &
     &          result (handle)                                         &
     &          bind(c, name='DLB_BarrierNamedRegister')
            use :: iso_c_binding
            type(c_ptr) :: handle
            character(kind=c_char), intent(in) :: barrier_name(*)
            integer(kind=c_int), value, intent(in) :: flags
        end function dlb_barriernamedregister_c
    end interface

    call string_f2c(barrier_name, barrier_name_c)

    handle = dlb_barriernamedregister_c(barrier_name_c, flags)

end function dlb_barriernamedregister


function dlb_barriernamedget(barrier_name, flags) result (handle)
    use :: iso_c_binding
    use :: mod_string, only: string_f2c
    implicit none
    type(c_ptr) :: handle
    character(len=*), intent(in) :: barrier_name
    integer(kind=c_int), value, intent(in) :: flags

    character(kind=c_char) :: barrier_name_c(len_trim(barrier_name)+1)

    interface
        function dlb_barriernamedget_c(barrier_name, flags)             &
     &          result (handle)                                         &
     &          bind(c, name='DLB_BarrierNamedGet')
            use :: iso_c_binding
            type(c_ptr) :: handle
            character(kind=c_char), intent(in) :: barrier_name(*)
            integer(kind=c_int), value, intent(in) :: flags
        end function dlb_barriernamedget_c
    end interface

    call string_f2c(barrier_name, barrier_name_c)

    handle = dlb_barriernamedget_c(barrier_name_c, flags)

end function dlb_barriernamedget


function dlb_setvariable(variable, val) result (ierr)
    use :: iso_c_binding
    use :: mod_string, only: string_f2c
    implicit none
    integer(kind=c_int) :: ierr
    character(len=*), intent(in) :: variable
    character(len=*), intent(in) :: val

    character(kind=c_char) :: variable_c(len_trim(variable)+1)
    character(kind=c_char) :: val_c(len_trim(val)+1)

    interface
        function dlb_setvariable_c(variable, val) result (ierr)         &
     &          bind(c, name='DLB_SetVariable')
            use :: iso_c_binding
            integer(kind=c_int) :: ierr
            character(kind=c_char), intent(in) :: variable(*)
            character(kind=c_char), intent(in) :: val(*)
        end function dlb_setvariable_c
    end interface

    call string_f2c(variable, variable_c)
    call string_f2c(val, val_c)

    ierr = dlb_setvariable_c(variable_c, val_c)

end function dlb_setvariable


function dlb_getvariable(variable, val) result (ierr)
    use :: iso_c_binding
    use :: mod_string, only: string_c2f, string_f2c
    implicit none
    integer(kind=c_int) :: ierr
    character(len=*), intent(in) :: variable
    character(len=*), intent(out) :: val

    integer, parameter :: MAX_OPTION_LENGTH = 64
    character(kind=c_char) :: val_c(MAX_OPTION_LENGTH)
    character(kind=c_char) :: variable_c(len_trim(variable)+1)

    interface
        function dlb_getvariable_c(variable, val) result (ierr)         &
     &          bind(c, name='DLB_GetVariable')
            use :: iso_c_binding
            integer(kind=c_int) :: ierr
            character(kind=c_char), intent(in) :: variable(*)
            character(kind=c_char), intent(out) :: val(*)
        end function dlb_getvariable_c
    end interface

    call string_f2c(variable, variable_c)

    ierr = dlb_getvariable_c(variable_c, val_c)

    call string_c2f(val_c, val)

end function dlb_getvariable


function dlb_drom_setprocessmaskstr(pid, mask, flags) result (ierr)
    use :: iso_c_binding
    use :: mod_string, only: string_f2c
    implicit none
    integer(kind=c_int) :: ierr
    integer(kind=c_int), value, intent(in) :: pid
    character(len=*), intent(in) :: mask
    integer(kind=c_int), value, intent(in) :: flags

    character(kind=c_char) :: mask_c(len_trim(mask)+1)

    interface
        function dlb_drom_setprocessmaskstr_c(pid, mask, flags)         &
     &          result (ierr)                                           &
     &          bind(c, name='DLB_DROM_SetProcessMaskStr')
            use :: iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: pid
            character(kind=c_char), intent(in) :: mask(*)
            integer(kind=c_int), value, intent(in) :: flags
        end function dlb_drom_setprocessmaskstr_c
    end interface

    call string_f2c(mask, mask_c)

    ierr = dlb_drom_setprocessmaskstr_c(pid, mask_c, flags)

end function dlb_drom_setprocessmaskstr


function dlb_mngo_regionregister(region_name, flags) result(handle)
    use :: iso_c_binding
    use :: mod_string, only: string_f2c
    implicit none
    type(c_ptr) :: handle
    character(len=*), intent(in) :: region_name
    integer(kind=c_int), value, intent(in) :: flags

    character(kind=c_char) :: region_name_c(len_trim(region_name)+1)

    interface
        function dlb_mngo_regionregister_c(region_name, flags)         &
    &           result(handle)                                         &
    &           bind(c, name="DLB_MNGO_RegionRegister")
            use :: iso_c_binding
            type(c_ptr) :: handle
            character(kind=c_char), intent(in) :: region_name(*)
            integer(kind=c_int), value, intent(in) :: flags
        end function dlb_mngo_regionregister_c
    end interface

    call string_f2c(region_name, region_name_c)

    handle = dlb_mngo_regionregister_c(region_name_c, flags)

end function dlb_mngo_regionregister


function dlb_talp_querypopnodemetrics(name, node_metrics) result(ierr)
    use :: iso_c_binding
    use :: mod_string, only: string_f2c
    implicit none
    include 'dlbf_types.h'
    integer(kind=c_int) :: ierr
    character(len=*), intent(in) :: name
    type(dlb_node_metrics_t), intent(out) :: node_metrics

    character(kind=c_char) :: name_c(len_trim(name)+1)

    interface
        function dlb_talp_querypopnodemetrics_c(name, node_metrics)     &
                result(ierr)                                            &
                bind(c, name='DLB_TALP_QueryPOPNodeMetrics')
            use :: iso_c_binding
            import :: dlb_node_metrics_t
            integer(kind=c_int) :: ierr
            character(kind=c_char), intent(in) :: name(*)
            type(dlb_node_metrics_t), intent(out) :: node_metrics
        end function dlb_talp_querypopnodemetrics_c
    end interface

    call string_f2c(name, name_c)

    ierr = dlb_talp_querypopnodemetrics_c(name_c, node_metrics)

end function dlb_talp_querypopnodemetrics


function dlb_monitoringregionregister(region_name) result (handle)
    use :: iso_c_binding
    use :: mod_string, only: string_f2c
    implicit none
    type(c_ptr) :: handle
    character(len=*), intent(in) :: region_name

    character(kind=c_char) :: region_name_c(len_trim(region_name)+1)

    interface
        function dlb_monitoringregionregister_c(region_name)            &
     &          result (handle)                                         &
     &          bind(c, name='DLB_MonitoringRegionRegister')
            use :: iso_c_binding
            type(c_ptr) :: handle
            character(kind=c_char), intent(in) :: region_name(*)
        end function dlb_monitoringregionregister_c
    end interface

    call string_f2c(region_name, region_name_c)

    handle = dlb_monitoringregionregister_c(region_name_c)

end function dlb_monitoringregionregister
