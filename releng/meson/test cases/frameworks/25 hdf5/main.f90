use hdf5

implicit none

integer :: ier, major, minor, rel

call h5open_f(ier)
if (ier /= 0) error stop 'Unable to initialize HDF5'

call h5get_libversion_f(major, minor, rel, ier)
if (ier /= 0) error stop 'Unable to check HDF5 version'
print '(A,I1,A1,I0.2,A1,I1)','Fortran HDF5 version ',major,'.',minor,'.',rel

call h5close_f(ier)
if (ier /= 0) error stop 'Unable to close HDF5 library'

end program
