module dynamic
implicit none

private
public :: hello

interface hello
  module procedure say
end interface hello

contains

subroutine say
  print *, "Hello from shared library."
end subroutine say

end module dynamic
