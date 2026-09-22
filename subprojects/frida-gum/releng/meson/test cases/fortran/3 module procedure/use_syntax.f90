module circle
implicit none

integer :: x
real :: radius

interface default
  module procedure timestwo
end interface

contains

elemental integer function timestwo(x) result(y)
  integer, intent(in) :: x
   y = 2*x
end function
end module circle

program prog

use, non_intrinsic :: circle, only: timestwo, x

implicit none

x = 3

if (timestwo(x) /= 6) error stop 'fortran module procedure problem'

print *,'OK: Fortran module procedure'

end program prog
