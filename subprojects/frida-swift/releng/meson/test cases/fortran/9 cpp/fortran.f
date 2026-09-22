      function fortran() bind(C)
      use, intrinsic :: iso_c_binding, only: dp=>c_double
      implicit none

      real(dp) :: r, fortran

      call random_number(r)

      fortran = 2._dp**r

      end function fortran
