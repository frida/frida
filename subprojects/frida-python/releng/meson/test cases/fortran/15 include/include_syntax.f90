program test_include_syntax

implicit none

integer :: x, y

x = 1
y = 0

! include "timestwo.f90"

include "timestwo.f90"  ! inline comment check
if (x/=2) error stop 'failed on first include'

! leading space check
  include 'timestwo.f90'
if (x/=4) error stop 'failed on second include'

! Most Fortran compilers can't handle the non-standard #include,
! including (ha!) Flang, Gfortran, Ifort and PGI.
! #include "timestwo.f90"

print *, 'OK: Fortran include tests: x=',x

end program
