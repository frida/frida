program main
implicit none

integer :: i, j
integer, parameter :: N=3
real :: A(N,N)

A = 0

forall (i=1:N, j=1:N)
  A(i,j) = 1
end forall

end program
