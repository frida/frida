program main
use, intrinsic :: iso_fortran_env, only : error_unit
implicit none

! http://fortranwiki.org/fortran/show/Real+precision
integer, parameter :: sp = selected_real_kind(6, 37)
integer, parameter :: dp = selected_real_kind(15, 307)

real(sp) :: a32
real(dp) :: a64

real(sp), parameter :: pi32 = 4*atan(1._sp)
real(dp), parameter :: pi64 = 4*atan(1._dp)

if (pi32 == pi64) stop 1

call timestwo(a32)
call timestwo(a64)

contains

elemental subroutine timestwo(a)

class(*), intent(inout) :: a

select type (a)
  type is (real(sp))
    a = 2*a
  type is (real(dp))
    a = 2*a
  type is (integer)
    a = 2*a
end select

end subroutine timestwo

end program
