! submodule (bogus) foo
! testing don't detect commented submodule

submodule (a1:a2) a3  ! testing inline comment

contains

module procedure get_pi
  get_pi = 4.*atan(1.)
end procedure get_pi


end submodule a3
