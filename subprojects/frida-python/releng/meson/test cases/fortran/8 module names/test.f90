program main
use mymod1
use MyMod2  ! test inline comment

implicit none

integer, parameter :: testVar = myModVal1 + myModVal2

end program
