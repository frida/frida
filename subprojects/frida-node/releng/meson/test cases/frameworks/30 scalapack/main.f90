! minimal Scalapack demo
implicit none

integer :: ictxt, myid, nprocs, mycol, myrow, npcol, nprow
real :: eps
real, external :: pslamch

! arbitrary test parameters
npcol = 2
nprow = 2

call blacs_pinfo(myid, nprocs)
call blacs_get(-1, 0, ictxt)
call blacs_gridinit(ictxt, "C", nprocs, 1)

call blacs_gridinfo(ictxt, nprow, npcol, myrow, mycol)

eps = pslamch(ictxt, 'E')

if(myrow == mycol) print '(A, F10.6)', "OK: Scalapack Fortran  eps=", eps

call blacs_gridexit(ictxt)
call blacs_exit(0)

end program
