      program main
    ! non-integer loop indices are deleted in Fortran 95 standard
      real a

      do 10 a=0,0.5,0.1
10    continue

      end program
