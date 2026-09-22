module main_lib

  use static_hello
  implicit none

  private
  public :: main_hello

  contains

  subroutine main_hello
    call static_say_hello()
    print *, "Main hello routine finished."
  end subroutine main_hello

end module main_lib
