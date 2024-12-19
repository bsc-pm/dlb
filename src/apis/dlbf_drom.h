!-------------------------------------------------------------------------------!
!  Copyright 2009-2024 Barcelona Supercomputing Center                          !
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

       interface
        function dlb_drom_attach() result (ierr)                        &
     &          bind(c, name='DLB_DROM_Attach')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_drom_attach

        function dlb_drom_detach() result (ierr)                        &
     &          bind(c, name='DLB_DROM_Detach')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_drom_detach

        function dlb_drom_getnumcpus(ncpus) result (ierr)               &
     &          bind(c, name='DLB_DROM_GetNumCpus')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), intent(in) :: ncpus
        end function dlb_drom_getnumcpus

        function dlb_drom_setprocessmaskstr(pid, mask, flags)           &
     &          result (ierr)                                           &
     &          bind(c, name='DLB_DROM_SetProcessMaskStr')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: pid
            character(kind=c_char), intent(in) :: mask(*)
            integer(kind=c_int), value, intent(in) :: flags
        end function dlb_drom_setprocessmaskstr
      end interface

! -*- fortran -*-  vim: set ft=fortran:
