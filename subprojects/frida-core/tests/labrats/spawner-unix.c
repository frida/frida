#define _GNU_SOURCE

#ifndef __ANDROID__
# define HAVE_POSIX_SPAWN
#endif

#ifdef HAVE_TVOS
# include <Availability.h>
# undef __TVOS_PROHIBITED
# define __TVOS_PROHIBITED
# undef __API_UNAVAILABLE
# define __API_UNAVAILABLE(...)
# include <sys/syslimits.h>
#endif

#include <dlfcn.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#ifdef __linux__
# include <linux/limits.h>
#endif
#ifdef __APPLE__
# include <sys/param.h>
# define environ (* _NSGetEnviron ())
extern char *** _NSGetEnviron (void);
#else
extern char ** environ;
#endif
#ifdef HAVE_POSIX_SPAWN
# include <spawn.h>
#endif

static int spawn_child (const char * program, const char * method, bool exit_on_failure);
static int join_child (pid_t pid);

int
main (int argc, char * argv[])
{
  const char * operation;
  int result;

  if (argc < 3)
    goto missing_argument;

  operation = argv[1];

  if (strcmp (operation, "spawn") == 0)
  {
    const char * good_path = argv[0];
    const char * method = argv[2];
    const bool exit_on_failure = true;

    result = spawn_child (good_path, method, exit_on_failure);
  }
  else if (strcmp (operation, "spawn-bad-path") == 0)
  {
    const char * good_path = argv[0];
    char bad_path[PATH_MAX + 1];
    const char * method = argv[2];
    const bool exit_on_failure = true;

    sprintf (bad_path, "%s-does-not-exist", good_path);

    result = spawn_child (bad_path, method, exit_on_failure);
  }
  else if (strcmp (operation, "spawn-bad-then-good-path") == 0)
  {
    const char * good_path = argv[0];
    char bad_path[PATH_MAX + 1];
    const char * method = argv[2];
    const bool bad_exit_on_failure = false;
    const bool good_exit_on_failure = true;

    sprintf (bad_path, "%s-does-not-exist", good_path);

    spawn_child (bad_path, method, bad_exit_on_failure);

    result = spawn_child (good_path, method, good_exit_on_failure);
  }
  else if (strcmp (operation, "say") == 0)
  {
    const char * message = argv[2];

    puts (message);

    result = 0;
  }
  else
  {
    goto missing_argument;
  }

  return result;

missing_argument:
  {
    fprintf (stderr, "Missing argument\n");
    return 1;
  }
}

static int
spawn_child (const char * path, const char * method, bool exit_on_failure)
{
  char * argv[] = { (char *) path, "say", (char *) method, NULL };
  char ** envp = environ;
  const char * plus_start, * fork_flavor, * exec_flavor;
  int fork_flavor_length, fork_result;
  int (* execvpe_impl) (const char * file, char * const * argv, char * const * envp) = NULL;

  if (strncmp (method, "posix_spawn", 11) == 0)
  {
#ifdef HAVE_POSIX_SPAWN
    const char * posix_spawn_flavor;
    pid_t child_pid;
    posix_spawnattr_t * attrp;
# ifdef POSIX_SPAWN_SETEXEC
    posix_spawnattr_t attr;
# endif
    int spawn_result;

    plus_start = strchr (method, '+');
    if (plus_start != NULL)
      posix_spawn_flavor = plus_start + 1;
    else
      posix_spawn_flavor = NULL;

    if (posix_spawn_flavor != NULL)
    {
      if (strcmp (posix_spawn_flavor, "setexec") == 0)
      {
# ifdef POSIX_SPAWN_SETEXEC
        posix_spawnattr_init (&attr);
        posix_spawnattr_setflags (&attr, POSIX_SPAWN_SETEXEC);

        attrp = &attr;
# else
        goto not_available;
# endif
      }
      else
      {
        goto missing_argument;
      }
    }
    else
    {
      attrp = NULL;
    }

    if (method[11] == 'p')
      spawn_result = posix_spawnp (&child_pid, path, NULL, attrp, argv, envp);
    else
      spawn_result = posix_spawn (&child_pid, path, NULL, attrp, argv, envp);

    if (attrp != NULL)
      posix_spawnattr_destroy (attrp);

    if (spawn_result == -1)
      goto posix_spawn_failed;

    return join_child (child_pid);
#else
    goto not_available;
#endif
  }

  plus_start = strchr (method, '+');
  if (plus_start != NULL)
  {
    fork_flavor = method;
    fork_flavor_length = plus_start - method;
    exec_flavor = plus_start + 1;
  }
  else
  {
    fork_flavor = NULL;
    fork_flavor_length = 0;
    exec_flavor = method;
  }

  if (strcmp (exec_flavor, "execvpe") == 0)
  {
    execvpe_impl = dlsym (RTLD_DEFAULT, "execvpe");
  }

  if (fork_flavor != NULL)
  {
    if (strncmp (fork_flavor, "fork", fork_flavor_length) == 0)
      fork_result = fork ();
    else if (strncmp (fork_flavor, "vfork", fork_flavor_length) == 0)
      fork_result = vfork ();
    else
      goto missing_argument;
    if (fork_result == -1)
      goto fork_failed;

    if (fork_result > 0)
      return join_child (fork_result);
  }

  if (strcmp (exec_flavor, "execl") == 0)
  {
    execl (path, argv[0], argv[1], argv[2], (char *) NULL);
  }
  else if (strcmp (exec_flavor, "execlp") == 0)
  {
    execlp (path, argv[0], argv[1], argv[2], (char *) NULL);
  }
  else if (strcmp (exec_flavor, "execle") == 0)
  {
    execle (path, argv[0], argv[1], argv[2], (char *) NULL, envp);
  }
  else if (strcmp (exec_flavor, "execv") == 0)
  {
    execv (path, argv);
  }
  else if (strcmp (exec_flavor, "execvp") == 0)
  {
    execvp (path, argv);
  }
  else if (strcmp (exec_flavor, "execve") == 0)
  {
    execve (path, argv, envp);
  }
  else if (strcmp (exec_flavor, "execvpe") == 0)
  {
    if (execvpe_impl == NULL)
      goto not_available;
    execvpe_impl (path, argv, envp);
  }
  else
  {
    goto missing_argument;
  }

  fprintf (stderr, "%s failed: %s\n", exec_flavor, strerror (errno));
  if (exit_on_failure)
    _exit (1);

  return 1;

missing_argument:
  {
    fprintf (stderr, "Missing argument\n");
    return 1;
  }
#ifdef HAVE_POSIX_SPAWN
posix_spawn_failed:
  {
    fprintf (stderr, "Unable to spawn: %s\n", strerror (errno));
    return 1;
  }
#endif
fork_failed:
  {
    fprintf (stderr, "Unable to fork: %s\n", strerror (errno));
    return 1;
  }
not_available:
  {
    fprintf (stderr, "Not available on this OS\n");
    return 1;
  }
}

static int
join_child (pid_t pid)
{
  int status, wait_result;

  do
  {
    wait_result = waitpid (pid, &status, 0);
  }
  while (wait_result == -1 && errno == EINTR);

  return (wait_result == -1) ? 255 : status;
}
