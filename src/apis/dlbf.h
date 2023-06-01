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

       include 'dlbf-errors.h'

       interface
        function dlb_init(ncpus, mask, dlb_args) result (ierr)          &
     &          bind(c, name='DLB_Init')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: ncpus
            type(c_ptr), value, intent(in) :: mask
            character(kind=c_char), intent(in) :: dlb_args(*)
        end function dlb_init

        function dlb_finalize() result (ierr)                           &
     &          bind(c, name='DLB_Finalize')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_finalize

        function dlb_enable() result (ierr)                             &
     &          bind(c, name='DLB_Enable')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_enable

        function dlb_disable() result (ierr)                            &
     &          bind(c, name='DLB_Disable')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_disable

        function dlb_setmaxparallelism(maxp) result (ierr)              &
     &          bind(c, name='DLB_SetMaxParallelism')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: maxp
        end function dlb_setmaxparallelism

        function dlb_callbackset(which, callback) result (ierr)         &
     &          bind(c, name='DLB_CallbackSet')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: which
            type(c_funptr), value, intent(in) :: callback
        end function dlb_callbackset

        function dlb_callbackget(which, callback) result (ierr)         &
     &          bind(c, name='DLB_CallbackGet')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: which
            type(c_funptr), intent(out) :: callback
        end function dlb_callbackget

        function dlb_lend() result (ierr)                               &
     &          bind(c, name='DLB_Lend')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_lend

        function dlb_lendcpu(cpuid) result (ierr)                       &
     &          bind(c, name='DLB_LendCpu')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: cpuid
        end function dlb_lendcpu

        function dlb_lendcpus(ncpus) result (ierr)                      &
     &          bind(c, name='DLB_LendCpus')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: ncpus
        end function dlb_lendcpus

        function dlb_lendcpumask(mask) result (ierr)                    &
     &          bind(c, name='DLB_LendCpuMask')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: mask
        end function dlb_lendcpumask

        function dlb_reclaim() result (ierr)                            &
     &          bind(c, name='DLB_Reclaim')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_reclaim

        function dlb_reclaimcpu(cpuid) result (ierr)                    &
     &          bind(c, name='DLB_ReclaimCpu')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: cpuid
        end function dlb_reclaimcpu

        function dlb_reclaimcpus(ncpus) result (ierr)                   &
     &          bind(c, name='DLB_ReclaimCpus')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: ncpus
        end function dlb_reclaimcpus

        function dlb_reclaimcpumask(mask) result (ierr)                 &
     &          bind(c, name='DLB_ReclaimCpuMask')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: mask
        end function dlb_reclaimcpumask

        function dlb_acquirecpu(cpuid) result (ierr)                    &
     &          bind(c, name='DLB_AcquireCpu')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: cpuid
        end function dlb_acquirecpu

        function dlb_acquirecpus(ncpus) result (ierr)                   &
     &          bind(c, name='DLB_AcquireCpus')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: ncpus
        end function dlb_acquirecpus

        function dlb_acquirecpumask(mask) result (ierr)                 &
     &          bind(c, name='DLB_AcquireCpuMask')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: mask
        end function dlb_acquirecpumask

        function dlb_acquirecpusinmask(ncpus, mask) result (ierr)       &
     &          bind(c, name='DLB_AcquireCpusInMask')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: ncpus
            type(c_ptr), value, intent(in) :: mask
        end function dlb_acquirecpusinmask

        function dlb_borrow() result (ierr)                             &
     &          bind(c, name='DLB_Borrow')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_borrow

        function dlb_borrowcpu(cpuid) result (ierr)                     &
     &          bind(c, name='DLB_BorrowCpu')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: cpuid
        end function dlb_borrowcpu

        function dlb_borrowcpus(ncpus) result (ierr)                    &
     &          bind(c, name='DLB_BorrowCpus')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: ncpus
        end function dlb_borrowcpus

        function dlb_borrowcpumask(mask) result (ierr)                  &
     &          bind(c, name='DLB_BorrowCpuMask')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: mask
        end function dlb_borrowcpumask

        function dlb_borrowcpusinmask(ncpus, mask) result (ierr)        &
     &          bind(c, name='DLB_BorrowCpusInMask')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: ncpus
            type(c_ptr), value, intent(in) :: mask
        end function dlb_borrowcpusinmask

        function dlb_return() result (ierr)                             &
     &          bind(c, name='DLB_Return')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_return

        function dlb_returncpu(cpuid) result (ierr)                     &
     &          bind(c, name='DLB_ReturnCpu')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: cpuid
        end function dlb_returncpu

        function dlb_returncpumask(mask) result (ierr)                  &
     &          bind(c, name='DLB_ReturnCpuMask')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: mask
        end function dlb_returncpumask

        function dlb_polldrom(ncpus, mask) result (ierr)                &
     &          bind(c, name='DLB_PollDROM')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), intent(out) :: ncpus
            type(c_ptr), intent(out) :: mask
        end function dlb_polldrom

        function dlb_polldrom_update() result (ierr)                    &
     &          bind(c, name='DLB_PollDROM_Update')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_polldrom_update

        function dlb_checkcpuavailability(cpuid) result(ierr)           &
     &          bind(c, name='DLB_CheckCpuAvailability')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: cpuid
        end function dlb_checkcpuavailability

        function dlb_barrier() result (ierr)                            &
     &          bind(c, name='DLB_Barrier')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_barrier

        function dlb_barrierattach() result (ierr)                      &
     &          bind(c, name='DLB_BarrierAttach')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_barrierattach

        function dlb_barrierdetach() result (ierr)                      &
     &          bind(c, name='DLB_BarrierDetach')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_barrierdetach

        function dlb_setvariable(variable, val) result (ierr)           &
     &          bind(c, name='DLB_SetVariable')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            character(kind=c_char), intent(in) :: variable(*)
            character(kind=c_char), intent(in) :: val(*)
        end function dlb_setvariable

        function dlb_getvariable(variable, val) result (ierr)           &
     &          bind(c, name='DLB_GetVariable')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            character(kind=c_char), intent(in) :: variable(*)
            character(kind=c_char), intent(out) :: val(*)
        end function dlb_getvariable

        function dlb_printvariables() result (ierr)                     &
     &          bind(c, name='DLB_PrintVariables')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_printvariables

        function dlb_printshmem(num_columns) result (ierr)              &
     &          bind(c, name='DLB_PrintShmem')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: num_columns
        end function dlb_printshmem

        function dlb_strerror(errnum) result(str)                       &
     &          bind(c, name='DLB_Strerror')
            use iso_c_binding
            type(c_ptr) :: str
            integer(kind=c_int), value, intent(in) :: errnum
        end function dlb_strerror

      end interface

! -*- fortran -*-  vim: set ft=fortran:
