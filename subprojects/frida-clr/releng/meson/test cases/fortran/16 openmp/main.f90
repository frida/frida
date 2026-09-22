program main
use, intrinsic :: iso_fortran_env, only: stderr=>error_unit
use omp_lib, only: omp_get_max_threads
implicit none

integer :: N, ierr
character(80) :: buf  ! can't be allocatable in this use case. Just set arbitrarily large.

call get_environment_variable('OMP_NUM_THREADS', buf, status=ierr)
if (ierr/=0) error stop 'environment variable OMP_NUM_THREADS could not be read'
read(buf,*) N

if (omp_get_max_threads() /= N) then
  write(stderr, *) 'Max Fortran threads: ', omp_get_max_threads(), '!=', N
  error stop
endif

end program
