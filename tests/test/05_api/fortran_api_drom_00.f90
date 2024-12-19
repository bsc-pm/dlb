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
    include 'dlbf_drom.h'
    ! integer, parameter :: N = 100
    integer :: err, ncpus
    ! type(dlb_monitor_t), pointer :: dlb_monitor
    ! type(c_ptr) :: dlb_handle_1, dlb_handle_2, dlb_handle_3
    ! character(9), pointer :: monitor_name

    err = DLB_DROM_Attach()
    if (err /= DLB_SUCCESS) call abort

    err = DLB_DROM_GetNumCpus(ncpus)
    if (ncpus <= 0) call abort
    if (err /= DLB_SUCCESS) call abort

    err = DLB_DROM_SetProcessMaskStr(0, c_char_"0-3"//C_NULL_CHAR, 0)
    if (err /= DLB_ERR_NOPROC) call abort

    err = DLB_DROM_Detach()
    if (err /= DLB_SUCCESS) call abort

end program test
