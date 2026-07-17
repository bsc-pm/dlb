
module unbalanced
    use iso_c_binding
    include "mpif.h"
    include "omp_lib.h"
    include "dlbf.h"
    include "dlbf_mngo.h"
    include "dlbf_talp.h"

    type(c_ptr) :: dlb_region, monitor
    type(c_ptr) :: dlb_region_in, monitor_in
    integer :: ierror, comm_size, comm_rank
    integer :: a(1:5000, 1:5000)

contains
    subroutine prepare_mngo_test()
        integer :: i, j

        call MPI_Comm_size(MPI_COMM_WORLD, comm_size, ierror)
        call MPI_Comm_rank(MPI_COMM_WORLD, comm_rank, ierror)

        !$omp parallel do default(shared) private(j)
        do i = 1, 5000
            do j = 1, 5000
                a(j, i) = j * 5000 + i
            end do
        end do
        !$omp end parallel do

    end subroutine prepare_mngo_test

    subroutine test_mngo()
        integer :: i, j, k

        !$omp parallel do default(shared) private(j,k)
        do i = 2, 4999
            do j = 2, 4999
                do k = 1, 20
                    a(j, i) = (a(j-1, i) + a(j, i-1) + a(j+1, i) + a(j, i+1)) / 4
                end do
            end do
        end do
        !$omp end parallel do

        dlb_region = DLB_MNGO_RegionRegister("Region 1" // C_NULL_CHAR, DLB_DROM_REGION)

        call MPI_Barrier(MPI_COMM_WORLD, ierror)
        ierror = DLB_MNGO_RegionStart(dlb_region)

        ! !$omp parallel
        ! !$omp master
        ! print *, comm_rank, omp_get_num_threads()
        ! !$omp end master
        ! !$omp end parallel

        ! !$omp parallel
        ! !$omp master
        ! print *, comm_rank, omp_get_num_threads()
        ! !$omp end master
        ! !$omp end parallel

        !$omp parallel do default(shared) private(j,k) schedule(dynamic)
        do i = 2, 4999
            do j = 2, 4999
                a(j, i) = (a(j-1, i) + a(j, i-1) + a(j+1, i) + a(j, i+1)) / 4

                if ( comm_rank .eq. 2) then
                    do k = 1, 20
                    a(j, i) = (a(j-1, i) + a(j, i-1) + a(j+1, i) + a(j, i+1)) / 4
                    end do
                end if
                if ( comm_rank .ge. 3) then
                    do k = 1, 10
                    a(j, i) = (a(j-1, i) + a(j, i-1) + a(j+1, i) + a(j, i+1)) / 4
                    end do
                end if
            end do
        end do
        !$omp end parallel do

        call MPI_Barrier(MPI_COMM_WORLD, ierror)
        ierror = DLB_MNGO_RegionStop(dlb_region)

        dlb_region = DLB_MNGO_RegionRegister("Region 2" // C_NULL_CHAR, DLB_DROM_REGION)

        call MPI_Barrier(MPI_COMM_WORLD, ierror)
        ierror = DLB_MNGO_RegionStart(dlb_region)

        !$omp parallel do default(shared) private(j,k) schedule(dynamic)
        do i = 2, 4999
            do j = 2, 4999
                a(j, i) = (a(j-1, i) + a(j, i-1) + a(j+1, i) + a(j, i+1)) / 4

                if ( comm_rank .lt. 2) then
                    do k = 1, 200
                    a(j, i) = (a(j-1, i) + a(j, i-1) + a(j+1, i) + a(j, i+1)) / 4
                    end do
                end if

                if ( comm_rank .eq. 2) then
                    do k = 1, 10
                    a(j, i) = (a(j-1, i) + a(j, i-1) + a(j+1, i) + a(j, i+1)) / 4
                    end do
                end if
            end do
            
            do j = 2, 4999
                do k = 1, 20
                    a(j, i) = (a(j-1, i) + a(j, i-1) + a(j+1, i) + a(j, i+1)) / 4
                end do
            end do
        end do
        !$omp end parallel do

        call MPI_Barrier(MPI_COMM_WORLD, ierror)
        ierror = DLB_MNGO_RegionStop(dlb_region)

    end subroutine test_mngo
end module unbalanced 

program test
    use unbalanced
    implicit none

    integer :: i

    call MPI_Init(ierror)
    call prepare_mngo_test()

    do i = 1, 20
        call test_mngo()
    end do

    call MPI_Finalize(ierror)
end program test

