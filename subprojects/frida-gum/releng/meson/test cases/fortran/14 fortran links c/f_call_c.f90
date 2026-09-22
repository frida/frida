program main
implicit none

interface
subroutine hello()  bind (c)
end subroutine hello
end interface

call hello()

end program
