#ifdef HAVE_TVOS
# include <Availability.h>
# undef __TVOS_PROHIBITED
# define __TVOS_PROHIBITED
#endif

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

int
main (void)
{
  int fds[2];
  pid_t res;
  char ack;
  ssize_t n;

  pipe (fds);

  res = fork ();
  if (res != 0)
  {
    puts ("Parent speaking");

    do {
      n = read (fds[0], &ack, sizeof (ack));
    } while (n == -1 && errno == EINTR);
  }
  else
  {
    puts ("Child speaking");

    ack = 42;
    do {
      n = write (fds[1], &ack, sizeof (ack));
    } while (n == -1 && errno == EINTR);
  }

  return 0;
}
