!---------------------------------------------------------------------------------!
!   Copyright 2017 Barcelona Supercomputing Center                                !
!                                                                                 !
!   This file is part of the DLB library.                                         !
!                                                                                 !
!   DLB is free software: you can redistribute it and/or modify                   !
!   it under the terms of the GNU Lesser General Public License as published by   !
!   the Free Software Foundation, either version 3 of the License, or             !
!   (at your option) any later version.                                           !
!                                                                                 !
!   DLB is distributed in the hope that it will be useful,                        !
!   but WITHOUT ANY WARRANTY; without even the implied warranty of                !
!   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                 !
!   GNU Lesser General Public License for more details.                           !
!                                                                                 !
!   You should have received a copy of the GNU Lesser General Public License      !
!   along with DLB.  If not, see <http://www.gnu.org/licenses/>.                  !
!---------------------------------------------------------------------------------!

!<testinfo>
!   test_generator="gens/basic-generator"
!   test_generator_ENV=( "LB_TEST_MODE=single" )
!</testinfo>

program test
    use iso_c_binding
    implicit none
    include 'dlbf.h'
    integer :: err
    type(c_ptr) :: process_mask

    err = dlb_init(4, C_NULL_PTR, c_char_"--drom=1"//C_NULL_CHAR)
    if (err /= DLB_SUCCESS) call abort

    if (dlb_disable() /= DLB_SUCCESS ) call abort
    if (dlb_enable() /= DLB_SUCCESS ) call abort
    if (dlb_setmaxparallelism(1) /= DLB_SUCCESS ) call abort

    ! Lend
    if (dlb_lend() /= DLB_ERR_NOPOL ) call abort
    if (dlb_lendcpu(0) /= DLB_ERR_NOPOL ) call abort
    if (dlb_lendcpus(1) /= DLB_ERR_NOPOL ) call abort
    if (dlb_lendcpumask(process_mask) /= DLB_ERR_NOPOL ) call abort

    ! Reclaim
    if (dlb_reclaim() /= DLB_ERR_NOPOL ) call abort
    if (dlb_reclaimcpu(0) /= DLB_ERR_NOPOL ) call abort
    if (dlb_reclaimcpus(1) /= DLB_ERR_NOPOL ) call abort
    if (dlb_reclaimcpumask(process_mask) /= DLB_ERR_NOPOL ) call abort

    ! Acquire
    if (dlb_acquirecpu(0) /= DLB_ERR_NOPOL ) call abort
    if (dlb_acquirecpumask(process_mask) /= DLB_ERR_NOPOL ) call abort

    ! Borrow
    if (dlb_borrow() /= DLB_ERR_NOPOL ) call abort
    if (dlb_borrowcpu(1) /= DLB_ERR_NOPOL ) call abort
    if (dlb_borrowcpus(1) /= DLB_ERR_NOPOL ) call abort
    if (dlb_borrowcpumask(process_mask) /= DLB_ERR_NOPOL ) call abort

    ! Return
    if (dlb_return() /= DLB_ERR_NOPOL ) call abort
    if (dlb_returncpu(0) /= DLB_ERR_NOPOL ) call abort

    err = dlb_finalize()
    if (err /= DLB_SUCCESS) call abort
end program test
