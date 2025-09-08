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

#ifdef HAVE_MPI_COMM_SPAWN_F08
subroutine MPI_Comm_spawn_f08(command, argv, maxprocs, info, root, comm, intercomm, &
        array_of_errcodes, ierror)
    use :: mod_string, only: string_f2c
    use :: mpi_f08, only: MPI_Info, MPI_Comm, PMPI_Comm_spawn
    use, intrinsic :: iso_c_binding, only: c_loc, c_char

    implicit none
    character(len=*), intent(in), target :: command, argv(*)
    integer, intent(in) :: maxprocs, root
    type(MPI_Info), intent(in) :: info
    type(MPI_Comm), intent(in) :: comm
    type(MPI_Comm), intent(out) :: intercomm
    integer :: array_of_errcodes(*)
    integer, optional, intent(out) :: ierror

    character(kind=c_char) :: command_c(len_trim(command)+1)
    integer :: f08_ierror

    interface
        subroutine DLB_MPI_Comm_spawn_enter(command, argv, maxprocs, info, root, comm, &
                intercomm, array_of_errcodes) &
                bind(c, name="DLB_MPI_Comm_spawn_enter")
            use, intrinsic :: iso_c_binding, only: c_char, c_ptr, c_int
            implicit none
            character(kind=c_char), intent(in) :: command(*)
            type(c_ptr), value, intent(in) :: argv
            integer(c_int), value, intent(in) :: maxprocs
            integer(c_int), value, intent(in) :: info
            integer(c_int), value, intent(in) :: root
            integer(c_int), value, intent(in) :: comm
            integer(c_int), intent(out) :: intercomm
            integer(c_int) :: array_of_errcodes(*)
        end subroutine DLB_MPI_Comm_spawn_enter

        subroutine DLB_MPI_Comm_spawn_leave() &
                bind(c, name="DLB_MPI_Comm_spawn_leave")
        end subroutine DLB_MPI_Comm_spawn_leave
    end interface

    call string_f2c(command, command_c)

    call DLB_MPI_Comm_spawn_enter(command_c, c_loc(argv), maxprocs, info%MPI_VAL, root, &
            comm%MPI_VAL, intercomm%MPI_VAL, array_of_errcodes)

    call PMPI_Comm_spawn(command, argv, maxprocs, info, root, comm, intercomm, array_of_errcodes, &
        f08_ierror)

    if (present(ierror)) ierror = f08_ierror

    call DLB_MPI_Comm_spawn_leave()

end subroutine MPI_Comm_spawn_f08
#endif

#ifdef HAVE_MPI_COMM_SPAWN_MULTIPLE_F08
subroutine MPI_Comm_spawn_multiple_f08(count, array_of_commands, array_of_argv, array_of_maxprocs, &
        array_of_info, root, comm, intercomm, array_of_errcodes, ierror)
    use :: mod_string, only: string_f2c
    use :: mpi_f08, only: MPI_Info, MPI_Comm, PMPI_Comm_spawn_multiple
    use, intrinsic :: iso_c_binding, only: c_loc

    implicit none
    integer, intent(in) :: count, array_of_maxprocs(*), root
    character(len=*), intent(in), target :: array_of_commands(*), array_of_argv(count, *)
    type(MPI_Info), intent(in) :: array_of_info(*)
    type(MPI_Comm), intent(in) :: comm
    type(MPI_Comm), intent(out) :: intercomm
    integer :: array_of_errcodes(*)
    integer, optional, intent(out) :: ierror

    integer :: f08_ierror

    interface
        subroutine DLB_MPI_Comm_spawn_multiple_enter(count, array_of_commands, &
                array_of_argv, array_of_maxprocs, array_of_info, root, comm, intercomm, &
                array_of_errcodes) &
                bind(c, name="DLB_MPI_Comm_spawn_multiple_enter")
            use, intrinsic :: iso_c_binding, only: c_char, c_ptr, c_int
            implicit none
            integer(c_int), value, intent(in) :: count
            type(c_ptr), value, intent(in) :: array_of_commands
            type(c_ptr), value, intent(in) :: array_of_argv
            integer(c_int), intent(in) :: array_of_maxprocs(*)
            integer(c_int), intent(in) :: array_of_info(*)
            integer(c_int), value, intent(in) :: root
            integer(c_int), value, intent(in) :: comm
            integer(c_int), intent(out) :: intercomm
            integer(c_int) :: array_of_errcodes(*)
        end subroutine DLB_MPI_Comm_spawn_multiple_enter

        subroutine DLB_MPI_Comm_spawn_multiple_leave() &
                bind(c, name="DLB_MPI_Comm_spawn_multiple_leave")
        end subroutine DLB_MPI_Comm_spawn_multiple_leave
    end interface

    call DLB_MPI_Comm_spawn_multiple_enter(count, c_loc(array_of_commands), c_loc(array_of_argv), &
            array_of_maxprocs, array_of_info(1:count)%MPI_VAL, root, comm%MPI_VAL, &
            intercomm%MPI_VAL, array_of_errcodes)

    call PMPI_Comm_spawn_multiple(count, array_of_commands, array_of_argv, array_of_maxprocs, &
        array_of_info, root, comm, intercomm, array_of_errcodes, f08_ierror)

    if (present(ierror)) ierror = f08_ierror

    call DLB_MPI_Comm_spawn_multiple_leave()

end subroutine MPI_Comm_spawn_multiple_f08
#endif

#endif /* MPI_LIB */
