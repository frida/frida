module gzip

use iso_c_binding, only: c_char, c_ptr, c_int
implicit none

interface
type(c_ptr) function gzopen(path, mode) bind(C)
import c_char, c_ptr

character(kind=c_char), intent(in) :: path(*), mode(*)
end function gzopen
end interface

interface
integer(c_int) function gzwrite(file, buf, len) bind(C)
import c_int, c_ptr, c_char

type(c_ptr), value, intent(in) :: file
character(kind=c_char), intent(in) :: buf
integer(c_int), value, intent(in) :: len
end function gzwrite
end interface

interface
integer(c_int) function gzclose(file) bind(C)
import c_int, c_ptr

type(c_ptr), value, intent(in) :: file
end function gzclose
end interface

end module gzip
