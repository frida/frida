MODULE geom

type :: circle
   REAL :: Pi = 4.*atan(1.)
   REAL :: radius
end type circle
END MODULE geom

PROGRAM prog

use geom, only : circle
IMPLICIT NONE

type(circle) :: ell

ell%radius = 3.

END PROGRAM prog
