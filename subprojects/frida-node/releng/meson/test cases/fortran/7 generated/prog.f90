program generated
use mod2, only : modval1, modval2
use mod3, only : modval3
implicit none

if (modval1 + modval2 + modval3 /= 6) error stop

end program generated
