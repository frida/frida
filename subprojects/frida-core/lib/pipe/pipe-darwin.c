#include "frida-tvos.h"

#include "pipe-glue.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <mach/mach.h>

#define CHECK_MACH_RESULT(n1, cmp, n2, op) \
  if (!(n1 cmp n2)) \
  { \
    failed_operation = op; \
    goto mach_failure; \
  }
#define CHECK_BSD_RESULT(n1, cmp, n2, op) \
  if (!(n1 cmp n2)) \
  { \
    failed_operation = op; \
    goto bsd_failure; \
  }

typedef struct _FridaInitMessage FridaInitMessage;

struct _FridaInitMessage
{
  mach_msg_header_t header;
  mach_msg_trailer_t trailer;
};

extern int fileport_makeport (int fd, mach_port_t * port);
extern int fileport_makefd (mach_port_t port);

void
frida_pipe_transport_set_temp_directory (const gchar * path)
{
}

void *
_frida_pipe_transport_create_backend (gchar ** local_address, gchar ** remote_address, GError ** error)
{
  mach_port_t self_task;
  int status, sockets[2] = { -1, -1 }, i;
  kern_return_t kr;
  const gchar * failed_operation;
  mach_port_t local_wrapper = MACH_PORT_NULL;
  mach_port_t remote_wrapper = MACH_PORT_NULL;
  mach_port_t local_rx = MACH_PORT_NULL;
  mach_port_t local_tx = MACH_PORT_NULL;
  mach_port_t remote_rx = MACH_PORT_NULL;
  mach_port_t remote_tx = MACH_PORT_NULL;
  mach_msg_type_name_t acquired_type;
  mach_msg_header_t init;

  self_task = mach_task_self ();

  status = socketpair (AF_UNIX, SOCK_STREAM, 0, sockets);
  CHECK_BSD_RESULT (status, ==, 0, "socketpair");

  for (i = 0; i != G_N_ELEMENTS (sockets); i++)
  {
    int fd = sockets[i];
    const int no_sigpipe = TRUE;

    fcntl (fd, F_SETFD, FD_CLOEXEC);
    setsockopt (fd, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof (no_sigpipe));
    frida_unix_socket_tune_buffer_sizes (fd);
  }

  status = fileport_makeport (sockets[0], &local_wrapper);
  CHECK_BSD_RESULT (status, ==, 0, "fileport_makeport local");

  status = fileport_makeport (sockets[1], &remote_wrapper);
  CHECK_BSD_RESULT (status, ==, 0, "fileport_makeport remote");

  kr = mach_port_allocate (self_task, MACH_PORT_RIGHT_RECEIVE, &local_rx);
  CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_port_allocate local_rx");

  kr = mach_port_allocate (self_task, MACH_PORT_RIGHT_RECEIVE, &remote_rx);
  CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_port_allocate remote_rx");

  kr = mach_port_extract_right (self_task, local_rx, MACH_MSG_TYPE_MAKE_SEND, &remote_tx, &acquired_type);
  CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_port_extract_right remote_tx");

  kr = mach_port_extract_right (self_task, remote_rx, MACH_MSG_TYPE_MAKE_SEND, &local_tx, &acquired_type);
  CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_port_extract_right local_tx");

  init.msgh_size = sizeof (init);
  init.msgh_reserved = 0;
  init.msgh_id = 3;

  init.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_MOVE_SEND, MACH_MSG_TYPE_MOVE_SEND);
  init.msgh_remote_port = local_tx;
  init.msgh_local_port = local_wrapper;
  kr = mach_msg_send (&init);
  CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_msg_send local_tx");
  local_tx = MACH_PORT_NULL;
  local_wrapper = MACH_PORT_NULL;

  init.msgh_bits = MACH_MSGH_BITS (MACH_MSG_TYPE_MOVE_SEND, MACH_MSG_TYPE_MOVE_SEND);
  init.msgh_remote_port = remote_tx;
  init.msgh_local_port = remote_wrapper;
  kr = mach_msg_send (&init);
  CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_msg_send remote_tx");
  remote_tx = MACH_PORT_NULL;
  remote_wrapper = MACH_PORT_NULL;

  *local_address = g_strdup_printf ("pipe:port=0x%x", local_rx);
  *remote_address = g_strdup_printf ("pipe:port=0x%x", remote_rx);
  local_rx = MACH_PORT_NULL;
  remote_rx = MACH_PORT_NULL;

  goto beach;

mach_failure:
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Unexpected error while setting up mach ports (%s returned '%s')",
        failed_operation, mach_error_string (kr));
    goto beach;
  }
bsd_failure:
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Unexpected error while setting up mach ports (%s returned '%s')",
        failed_operation, g_strerror (errno));
    goto beach;
  }
beach:
  {
    guint i;

    if (remote_tx != MACH_PORT_NULL)
      mach_port_deallocate (self_task, remote_tx);
    if (local_tx != MACH_PORT_NULL)
      mach_port_deallocate (self_task, local_tx);

    if (remote_rx != MACH_PORT_NULL)
      mach_port_mod_refs (self_task, remote_rx, MACH_PORT_RIGHT_RECEIVE, -1);
    if (local_rx != MACH_PORT_NULL)
      mach_port_mod_refs (self_task, local_rx, MACH_PORT_RIGHT_RECEIVE, -1);

    if (remote_wrapper != MACH_PORT_NULL)
      mach_port_deallocate (self_task, remote_wrapper);
    if (local_wrapper != MACH_PORT_NULL)
      mach_port_deallocate (self_task, local_wrapper);

    for (i = 0; i != G_N_ELEMENTS (sockets); i++)
    {
      int fd = sockets[i];
      if (fd != -1)
        close (fd);
    }

    return NULL;
  }
}

void
_frida_pipe_transport_destroy_backend (void * backend)
{
}

gint
_frida_darwin_pipe_consume_stashed_file_descriptor (const gchar * address, GError ** error)
{
  gint fd = -1;
  G_GNUC_UNUSED gint assigned;
  mach_port_t port = MACH_PORT_NULL;
  FridaInitMessage init = { { 0, }, { 0, } };
  kern_return_t kr;
  const gchar * failed_operation;
  mach_port_t wrapper;

  assigned = sscanf (address, "pipe:port=0x%x", &port);
  g_assert (assigned == 1);

  kr = mach_msg (&init.header, MACH_RCV_MSG, 0, sizeof (init), port, 1, MACH_PORT_NULL);
  CHECK_MACH_RESULT (kr, ==, KERN_SUCCESS, "mach_msg");
  wrapper = init.header.msgh_remote_port;

  fd = fileport_makefd (wrapper);
  CHECK_BSD_RESULT (fd, !=, -1, "fileport_makefd");

  goto beach;

mach_failure:
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Unexpected error while setting up pipe (%s returned '%s')",
        failed_operation, mach_error_string (kr));
    goto beach;
  }
bsd_failure:
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Unexpected error while setting up pipe (%s returned '%s')",
        failed_operation, g_strerror (errno));
    goto beach;
  }
beach:
  {
    mach_msg_destroy (&init.header);

    mach_port_mod_refs (mach_task_self (), port, MACH_PORT_RIGHT_RECEIVE, -1);

    return fd;
  }
}
