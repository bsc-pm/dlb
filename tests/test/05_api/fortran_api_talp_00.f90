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

!<testinfo>
!   test_generator="gens/fortran-generator"
!</testinfo>

program test
    use iso_c_binding
    implicit none
    include 'dlbf.h'
    include 'dlbf_talp.h'
    integer, parameter :: N = 100
    integer :: i, err
    type(dlb_monitor_t), pointer :: dlb_monitor
    type(c_ptr) :: dlb_handle_1, dlb_handle_2, dlb_handle_3
    character(9), pointer :: monitor_name

    err = dlb_init(0, C_NULL_PTR, c_char_"--talp"//C_NULL_CHAR)
    if (err /= DLB_SUCCESS) call abort

    dlb_handle_1 = DLB_MonitoringRegionRegister(c_char_"region 1"//C_NULL_CHAR)
    if (.not. c_associated(dlb_handle_1)) call abort
    dlb_handle_2 = DLB_MonitoringRegionRegister(c_char_"region 2"//C_NULL_CHAR)
    if (.not. c_associated(dlb_handle_2)) call abort
    dlb_handle_3 = DLB_MonitoringRegionRegister(C_NULL_CHAR)
    if (.not. c_associated(dlb_handle_3)) call abort

    do i=1, N
        ! monitorized region
        err = DLB_MonitoringRegionStart(dlb_handle_1)
        if (err /= DLB_SUCCESS) call abort

        ! nested monitoring region
        err = DLB_MonitoringRegionStart(dlb_handle_2)
        if (err /= DLB_SUCCESS) call abort
        err = DLB_MonitoringRegionStop(dlb_handle_2)
        if (err /= DLB_SUCCESS) call abort

        ! nested anonymous monitoring region
        err = DLB_MonitoringRegionStart(dlb_handle_3)
        if (err /= DLB_SUCCESS) call abort
        err = DLB_MonitoringRegionStop(dlb_handle_3)
        if (err /= DLB_SUCCESS) call abort

        ! end monitoring region
        err = DLB_MonitoringRegionStop(dlb_handle_1)
        if (err /= DLB_SUCCESS) call abort
    enddo

    call c_f_pointer(dlb_handle_1, dlb_monitor)
    call c_f_pointer(dlb_monitor%name_, monitor_name)

    print *, monitor_name
    print *, dlb_monitor%num_measurements
    print *, dlb_monitor%num_resets
    print *, dlb_monitor%elapsed_time

    if (dlb_monitor%num_measurements /= N) call abort

    err = dlb_monitoringregionreport(dlb_handle_1)
    if (err /= DLB_SUCCESS) call abort
    err = dlb_monitoringregionreport(dlb_handle_2)
    if (err /= DLB_SUCCESS) call abort
    err = dlb_monitoringregionreport(dlb_handle_3)
    if (err /= DLB_SUCCESS) call abort

    err = dlb_finalize()
    if (err /= DLB_SUCCESS) call abort
end program test
