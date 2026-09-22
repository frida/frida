program main
implicit none

if (this_image() == 1)  print *, 'number of Fortran coarray images:', num_images()

sync all ! semaphore, ensures message above is printed at top.

print *, 'Process ', this_image()

end program
