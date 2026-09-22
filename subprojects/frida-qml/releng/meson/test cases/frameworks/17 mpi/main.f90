use, intrinsic :: iso_fortran_env, only: stderr=>error_unit
use mpi

implicit none

logical :: flag
integer :: ier

call MPI_Init(ier)

if (ier /= 0) then
  write(stderr,*) 'Unable to initialize MPI', ier
  stop 1
endif

call MPI_Initialized(flag, ier)
if (ier /= 0) then
  write(stderr,*) 'Unable to check MPI initialization state: ', ier
  stop 1
endif

call MPI_Finalize(ier)
if (ier /= 0) then
  write(stderr,*) 'Unable to finalize MPI: ', ier
  stop 1
endif

print *, "OK: Fortran MPI"

end program
