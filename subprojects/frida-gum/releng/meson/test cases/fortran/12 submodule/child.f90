submodule (parent) child

contains

module procedure pi2tau
  pi2tau = 2*pi
end procedure pi2tau

module procedure good
print *, 'Good!'
end procedure good

end submodule child
