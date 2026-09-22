program main
use iso_fortran_env, only: stderr=>error_unit
use iso_c_binding, only: c_int, c_char, c_null_char, c_ptr
use gzip, only: gzopen, gzwrite, gzclose

implicit none

character(kind=c_char,len=*), parameter :: path = c_char_"test.gz"//c_null_char
character(kind=c_char,len=*), parameter :: mode = c_char_"wb9"//c_null_char
integer(c_int), parameter :: buffer_size = 512

type(c_ptr) :: file
character(kind=c_char, len=buffer_size) :: buffer
integer(c_int) :: ret
integer :: i

! open file
file = gzopen(path, mode)

! fill buffer with data
do i=1,buffer_size/4
   write(buffer(4*(i-1)+1:4*i), '(i3.3, a)') i, new_line('')
end do
ret = gzwrite(file, buffer, buffer_size)
if (ret /= buffer_size) then
   write(stderr,'(a, i3, a, i3, a)') 'Error: ', ret, ' / ', buffer_size, &
        ' bytes written.'
   stop 1
end if

! close file
ret = gzclose(file)
if (ret /= 0) then
   write(stderr,*) 'Error: failure to close file with error code ', ret
   stop 1
end if

end program
