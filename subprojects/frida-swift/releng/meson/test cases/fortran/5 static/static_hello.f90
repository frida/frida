module static_hello
implicit none

private
public :: static_say_hello

interface static_say_hello
  module procedure say_hello
end interface static_say_hello

contains

subroutine say_hello
  print *, "Static library called."
end subroutine say_hello

end module static_hello
