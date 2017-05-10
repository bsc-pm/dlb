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

integer(kind=c_int), parameter :: DLB_NOUPDT        = 2
integer(kind=c_int), parameter :: DLB_NOTED         = 1
integer(kind=c_int), parameter :: DLB_SUCCESS       = 0
integer(kind=c_int), parameter :: DLB_ERR_UNKNOWN   = -1
integer(kind=c_int), parameter :: DLB_ERR_NOINIT    = -2
integer(kind=c_int), parameter :: DLB_ERR_INIT      = -3
integer(kind=c_int), parameter :: DLB_ERR_DISBLD    = -4
integer(kind=c_int), parameter :: DLB_ERR_NOSHMEM   = -5
integer(kind=c_int), parameter :: DLB_ERR_NOPROC    = -6
integer(kind=c_int), parameter :: DLB_ERR_PDIRTY    = -7
integer(kind=c_int), parameter :: DLB_ERR_PERM      = -8
integer(kind=c_int), parameter :: DLB_ERR_TIMEOUT   = -9
integer(kind=c_int), parameter :: DLB_ERR_NOCBK     = -10
integer(kind=c_int), parameter :: DLB_ERR_NOENT     = -11
integer(kind=c_int), parameter :: DLB_ERR_NOCOMP    = -12
integer(kind=c_int), parameter :: DLB_ERR_REQST     = -13
integer(kind=c_int), parameter :: DLB_ERR_NOMEM     = -14
integer(kind=c_int), parameter :: DLB_ERR_NOPOL     = -15
