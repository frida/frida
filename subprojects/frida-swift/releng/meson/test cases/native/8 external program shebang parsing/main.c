#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

#ifdef _WIN32
 #include <io.h>
 #include <windows.h>
#else
 #include <unistd.h>
#endif

/* Who cares about stack sizes in test programs anyway */
#define LINE_LENGTH 4096

static int
intrp_copyfile (char * src, char * dest)
{
#ifdef _WIN32
  if (!CopyFile (src, dest, FALSE))
    return 1;
  return 0;
#else
  return execlp ("cp", "cp", src, dest, NULL);
#endif
}

static void
parser_get_line (FILE * f, char line[LINE_LENGTH])
{
  if (!fgets (line, LINE_LENGTH, f))
    fprintf (stderr, "%s\n", strerror (errno));
}

int
main (int argc, char * argv[])
{
  FILE *f = NULL;
  char line[LINE_LENGTH];

  if (argc != 4) {
    fprintf (stderr, "Invalid number of arguments: %i\n", argc);
    goto err;
  }

  if ((f = fopen (argv[1], "r")) == NULL) {
    fprintf (stderr, "%s\n", strerror (errno));
    goto err;
  }

  parser_get_line (f, line);

  if (!line || line[0] != '#' || line[1] != '!') {
    fprintf (stderr, "Invalid script\n");
    goto err;
  }

  parser_get_line (f, line);

  if (!line || strncmp (line, "copy", 4) != 0) {
    fprintf (stderr, "Syntax error: %s\n", line);
    goto err;
  }

  return intrp_copyfile (argv[2], argv[3]);

err:
  fclose (f);
  return 1;
}
