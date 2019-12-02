! -*- fortran -*-  vim: set ft=fortran:
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

interface talp

        function dlb_talp_attach()
                result(ierr) bind(c,name='dlb_talp_attach')
                use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_talp_attach

        function dlb_talp_detach()
                result(ierr) bind(c,name='dlb_talp_detach')
                use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_talp_detach

        function dlb_talp_getnumcpus(ncpus)
                result(ierr) bind(c,name='dlb_talp_getnumcpus')
                use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(out) :: ncpus
        end function dlb_talp_getnumcpus

        function dlb_talp_getmpitime(process, mpi_time)
                result(ierr) bind(c,name='dlb_talp_getmpitime')
                use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: process
            type(c_ptr), value, intent(out) :: mpi_time
        end function dlb_talp_getmpitime

        function dlb_talp_getcputime(process, compute_time)
                result(ierr) bind(c,name='dlb_talp_getcputime')
                use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: process
            type(c_ptr), value, intent(out) :: compute_time
        end function dlb_talp_getcputime


end interface talp
