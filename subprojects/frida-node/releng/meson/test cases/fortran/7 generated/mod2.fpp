module mod2
use mod1, only : modval1
implicit none

integer, parameter :: modval2 = @TWO@

end module mod2
