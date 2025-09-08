module mod_string

    use, intrinsic :: iso_c_binding, only: c_char, c_null_char

    public :: string_c2f
    public :: string_f2c

contains

subroutine string_c2f(c_string, f_string)
    implicit none
    character(kind=c_char), intent(in) :: c_string(:)
    character(len=*), intent(out) :: f_string

    integer :: size_c_string, len_c_string, len_f_string, i

    size_c_string = size(c_string)

    i = 1
    do while (i <= size_c_string .and. c_string(i) /= c_null_char)
        f_string(i:i) = c_string(i)
        i = i + 1
    end do

    ! Fill the rest of f_string with spaces
    len_c_string = i
    len_f_string = len(f_string)
    if (len_c_string < len_f_string) then
        f_string(len_c_string:len_f_string) = ' '
    end if

end subroutine string_c2f

subroutine string_f2c(f_string, c_string)
    implicit none
    character(len=*), intent(in) :: f_string
    character(kind=c_char), intent(out) :: c_string(:)

    integer :: start_idx, end_idx, i, j

    ! MPI Info object requires stripping leading and trailing spaces
    start_idx = 1
    end_idx = len_trim(f_string)
    do while (start_idx <= end_idx .and. f_string(start_idx:start_idx) == ' ')
        start_idx = start_idx + 1
    end do

    j = 1
    do i = start_idx, end_idx
        c_string(j) = f_string(i:i)
        j = j + 1
    end do

    c_string(j) = c_null_char

end subroutine string_f2c

end module mod_string
