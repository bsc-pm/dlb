!-------------------------------------------------------------------------------!
!  Copyright 2009-2023 Barcelona Supercomputing Center                          !
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
    integer, parameter :: N = 100
    integer :: err
    type(c_ptr) :: dlb_barrier_1, dlb_barrier_2
    ! character(9), pointer :: barrier_name

    err = dlb_init(0, C_NULL_PTR, c_char_"--barrier"//C_NULL_CHAR)
    if (err /= DLB_SUCCESS) call abort

    dlb_barrier_1 = DLB_BarrierNamedRegister(                       &
        c_char_"barrier 1"//C_NULL_CHAR, DLB_BARRIER_LEWI_OFF)
    if (.not. c_associated(dlb_barrier_1)) call abort
    dlb_barrier_2 = DLB_BarrierNamedRegister(                       &
        c_char_"barrier 2"//C_NULL_CHAR, DLB_BARRIER_LEWI_ON)
    if (.not. c_associated(dlb_barrier_2)) call abort

    err = DLB_BarrierNamedAttach(dlb_barrier_2);
    if (err /= DLB_ERR_PERM) call abort

    err = DLB_BarrierNamed(dlb_barrier_2);
    if (err /= DLB_SUCCESS) call abort

    err = DLB_Printshmem(0, DLB_COLOR_AUTO);
    if (err /= DLB_SUCCESS) call abort

    err = DLB_BarrierNamedDetach(dlb_barrier_1)
    if (err /= DLB_SUCCESS) call abort

    err = DLB_BarrierNamed(dlb_barrier_1);
    if (err /= DLB_NOUPDT) call abort

    err = DLB_Printshmem(0, DLB_COLOR_AUTO);
    if (err /= DLB_SUCCESS) call abort

    err = dlb_finalize()
    if (err /= DLB_SUCCESS) call abort
end program test
