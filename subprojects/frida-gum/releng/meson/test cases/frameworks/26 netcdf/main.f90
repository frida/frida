use netcdf

implicit none

integer :: ncid

call check( nf90_create("foo.nc", NF90_CLOBBER, ncid) )

call check( nf90_close(ncid) )

contains

 subroutine check(status)
  integer, intent (in) :: status

  if(status /= nf90_noerr) error stop trim(nf90_strerror(status))
end subroutine check

end program
