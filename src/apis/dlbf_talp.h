!-------------------------------------------------------------------------------!
!  Copyright 2009-2025 Barcelona Supercomputing Center                          !
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

       include 'dlbf_types.h'

       interface

        !---------------------------------------------------------------------------!
        ! The following functions are intended to be called from 1st-party or
        ! 3rd-party programs indistinctly; that is, DLB applications, or external
        ! profilers as long as they invoke DLB_TALP_Attach.
        !---------------------------------------------------------------------------!

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

        function dlb_talp_querypopnodemetrics(name, node_metrics)       &
                result(ierr)
            use iso_c_binding
            import :: dlb_node_metrics_t
            integer(kind=c_int) :: ierr
            character(len=*), intent(in) :: name
            type(dlb_node_metrics_t), intent(out) :: node_metrics
        end function dlb_talp_querypopnodemetrics


        !---------------------------------------------------------------------------!
        ! The functions declared below are intended to be called only from
        ! 1st-party programs, and they should return an error if they are
        ! called from external profilers.
        !---------------------------------------------------------------------------!

        function dlb_monitoringregiongetglobal()                        &
     &          result (handle)                                         &
     &          bind(c, name='DLB_MonitoringRegionGetGlobal')
            use iso_c_binding
            type(c_ptr) :: handle
        end function dlb_monitoringregiongetglobal

        !! deprecated: bind to DLB_MonitoringRegionGetGlobal()
        function dlb_monitoringregiongetimplicit()                      &
     &          result (handle)                                         &
     &          bind(c, name='DLB_MonitoringRegionGetGlobal')
            use iso_c_binding
            type(c_ptr) :: handle
        end function dlb_monitoringregiongetimplicit

        function dlb_monitoringregionregister(region_name)              &
     &          result (handle)
            use iso_c_binding
            type(c_ptr) :: handle
            character(len=*), intent(in) :: region_name
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

        function dlb_monitoringregionupdate()                           &
     &         result (ierr) bind(c, name='DLB_MonitoringRegionsUpdate')
            use iso_c_binding
            integer(kind=c_int) :: ierr
        end function dlb_monitoringregionupdate

        function dlb_talp_collectpopmetrics(monitor, pop_metrics)      &
                result (ierr) bind(c, name='DLB_TALP_CollectPOPMetrics')
            use iso_c_binding
            import :: dlb_pop_metrics_t
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: monitor
            type(dlb_pop_metrics_t), intent(out) :: pop_metrics
        end function dlb_talp_collectpopmetrics

        function dlb_talp_collectpopnodemetrics(monitor, node_metrics)  &
                result (ierr) bind(c, name='DLB_TALP_CollectPOPNodeMetrics')
            use iso_c_binding
            import :: dlb_node_metrics_t
            integer(kind=c_int) :: ierr
            type(c_ptr), value, intent(in) :: monitor
            type(dlb_node_metrics_t), intent(out) :: node_metrics
        end function dlb_talp_collectpopnodemetrics
      end interface

! -*- fortran -*-  vim: set ft=fortran:
