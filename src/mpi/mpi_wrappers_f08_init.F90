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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef MPI_LIB

#include "mpi/config_mpi.h"

#ifdef HAVE_MPI_INIT_F08
subroutine MPI_Init_f08(ierror)
    use :: mpi_f08, only: PMPI_Init
    use, intrinsic :: iso_c_binding, only : c_null_ptr

    implicit none
    integer, optional, intent(out) :: ierror

    integer :: f08_ierror

    interface
        subroutine DLB_MPI_Init_enter(argc, argv) &
                bind(c, name="DLB_MPI_Init_enter")
            use, intrinsic :: iso_c_binding, only: c_ptr
            implicit none
            type(c_ptr), value, intent(in) :: argc
            type(c_ptr), value, intent(in) :: argv
        end subroutine DLB_MPI_Init_enter

        subroutine DLB_MPI_Init_leave() &
                bind(c, name="DLB_MPI_Init_leave")
        end subroutine DLB_MPI_Init_leave
    end interface

    call DLB_MPI_Init_enter(c_null_ptr, c_null_ptr)

    call PMPI_Init(f08_ierror)

    if (present(ierror)) ierror = f08_ierror

    call DLB_MPI_Init_leave()

end subroutine MPI_Init_f08
#endif

#ifdef HAVE_INIT_THREAD_F08
subroutine MPI_Init_thread_f08(required, provided, ierror)
    use :: mpi_f08, only: PMPI_Init_thread
    use, intrinsic :: iso_c_binding, only : c_null_ptr

    implicit none
    integer, intent(in) :: required
    integer, intent(out) :: provided
    integer, optional, intent(out) :: ierror

    integer :: f08_ierror

    interface
        subroutine DLB_MPI_Init_thread_enter(argc, argv, required, provided) &
                bind(c, name="DLB_MPI_Init_thread_enter")
            use, intrinsic :: iso_c_binding, only: c_ptr, c_int
            implicit none
            type(c_ptr), value, intent(in) :: argc
            type(c_ptr), value, intent(in) :: argv
            integer(c_int), value, intent(in) :: required
            integer(c_int), intent(out) :: provided
        end subroutine DLB_MPI_Init_thread_enter

        subroutine DLB_MPI_Init_thread_leave() &
                bind(c, name="DLB_MPI_Init_thread_leave")
        end subroutine DLB_MPI_Init_thread_leave
    end interface

    call DLB_MPI_Init_thread_enter(c_null_ptr, c_null_ptr, required, provided)

    call PMPI_Init_thread(required, provided, f08_ierror)

    if (present(ierror)) ierror = f08_ierror

    call DLB_MPI_Init_thread_leave()

end subroutine MPI_Init_thread_f08
#endif

#endif /* MPI_LIB */
