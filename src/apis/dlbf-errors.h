!-------------------------------------------------------------------------------!
!  Copyright 2009-2018 Barcelona Supercomputing Center                          !
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

integer, parameter :: DLB_NOUPDT        = 2
integer, parameter :: DLB_NOTED         = 1
integer, parameter :: DLB_SUCCESS       = 0
integer, parameter :: DLB_ERR_UNKNOWN   = -1
integer, parameter :: DLB_ERR_NOINIT    = -2
integer, parameter :: DLB_ERR_INIT      = -3
integer, parameter :: DLB_ERR_DISBLD    = -4
integer, parameter :: DLB_ERR_NOSHMEM   = -5
integer, parameter :: DLB_ERR_NOPROC    = -6
integer, parameter :: DLB_ERR_PDIRTY    = -7
integer, parameter :: DLB_ERR_PERM      = -8
integer, parameter :: DLB_ERR_TIMEOUT   = -9
integer, parameter :: DLB_ERR_NOCBK     = -10
integer, parameter :: DLB_ERR_NOENT     = -11
integer, parameter :: DLB_ERR_NOCOMP    = -12
integer, parameter :: DLB_ERR_REQST     = -13
integer, parameter :: DLB_ERR_NOMEM     = -14
integer, parameter :: DLB_ERR_NOPOL     = -15

! -*- fortran -*-  vim: set ft=fortran:
