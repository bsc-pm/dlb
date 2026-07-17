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

       integer, parameter ::DLB_LEWI_REGION     = 1
       integer, parameter ::DLB_DROM_REGION     = 2

       interface
        function dlb_mngo_regionregister(region_name, flags)           &
    &           result(handle)                                         &
    &           bind(c, name="DLB_MNGO_RegionRegister")
            use iso_c_binding
            type(c_ptr) :: handle
            character(kind=c_char), intent(in) :: region_name(*)
            integer(kind=c_int), value, intent(in) :: flags
        end function dlb_mngo_regionregister

        function dlb_mngo_regionstart(handle) result(ierror)            &
    &           bind(c, name="DLB_MNGO_RegionStart")
            use iso_c_binding
            integer(kind=c_int) :: ierror
            type(c_ptr), value, intent(in) :: handle
        end function dlb_mngo_regionstart

        function dlb_mngo_regionstop(handle) result(ierror)            &
    &           bind(c, name="DLB_MNGO_RegionStop")
            use iso_c_binding
            integer(kind=c_int) :: ierror
            type(c_ptr), value, intent(in) :: handle
        end function dlb_mngo_regionstop
       end interface

! -*- fortran -*-  vim: set ft=fortran:

