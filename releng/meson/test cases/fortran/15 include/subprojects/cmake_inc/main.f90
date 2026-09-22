program test_subproject_inc

implicit none

include 'thousand.f90'

if (thousand /= 1000) error stop 'did not include properly'

end program
