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

       type, bind(c) :: dlb_monitor_t
        type(c_ptr)             :: name_
        integer(kind=c_int)     :: num_measurements
        integer(kind=c_int)     :: num_resets
        integer(kind=c_int64_t) :: start_time
        integer(kind=c_int64_t) :: stop_time
        integer(kind=c_int64_t) :: elapsed_time
        integer(kind=c_int64_t) :: elapsed_computation_time
        integer(kind=c_int64_t) :: accumulated_MPI_time
        integer(kind=c_int64_t) :: accumulated_computation_time
        type(c_ptr)             :: data_
       end type

       interface
        function dlb_talp_attach() result(ierr)                         &
     &          bind(c,name='DLB_TALP_Attach')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_talp_attach

        function dlb_talp_detach() result(ierr)                         &
     &          bind(c,name='DLB_TALP_Detach')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_talp_detach

        function dlb_talp_getnumcpus(ncpus) result(ierr)                &
     &          bind(c,name='DLB_TALP_GetNumCPUs')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            real(c_double), intent(out) :: ncpus
        end function dlb_talp_getnumcpus

        function dlb_talp_getmpitime(process, mpi_time) result(ierr)    &
     &          bind(c,name='DLB_TALP_MPITimeGet')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: process
            real(c_double), intent(out) :: mpi_time
        end function dlb_talp_getmpitime

        function dlb_talp_getcputime(process, compute_time)             &
     &          result(ierr) bind(c,name='DLB_TALP_CPUTimeGet')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            integer(kind=c_int), value, intent(in) :: process
            real(c_double), intent(out) :: compute_time
        end function dlb_talp_getcputime

        function dlb_monitoringregionregister(region_name)              &
     &          result (handle)                                         &
                bind(c, name='DLB_MonitoringRegionRegister')
            use iso_c_binding
            type(c_ptr) :: handle
            character(kind=c_char), intent(in) :: region_name(*)
        end function dlb_monitoringregionregister

        function dlb_monitoringregionreset(handle)                      &
     &         result (ierr) bind(c, name='DLB_MonitoringRegionReset')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: handle
        end function dlb_monitoringregionreset

        function dlb_monitoringregionstart(handle)                      &
     &         result (ierr) bind(c, name='DLB_MonitoringRegionStart')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: handle
        end function dlb_monitoringregionstart

        function dlb_monitoringregionstop(handle)                       &
     &         result (ierr) bind(c, name='DLB_MonitoringRegionStop')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: handle
        end function dlb_monitoringregionstop

        function dlb_monitoringregionreport(handle)                     &
     &         result (ierr) bind(c, name='DLB_MonitoringRegionReport')
            use iso_c_binding
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: handle
        end function dlb_monitoringregionreport
      end interface

! -*- fortran -*-  vim: set ft=fortran:
