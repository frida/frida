module a1
implicit none

interface
module elemental real function pi2tau(pi)
  real, intent(in) :: pi
end function pi2tau

module real function get_pi()
end function get_pi
end interface

end module a1

program hierN

use a1
real :: tau, pi

pi = get_pi()

tau = pi2tau(pi)

print *,'pi=',pi,'tau=',tau

end program
