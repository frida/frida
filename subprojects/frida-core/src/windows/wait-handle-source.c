#include "frida-helper-backend.h"

#include <windows.h>

#define FRIDA_WAIT_HANDLE_SOURCE(s) ((FridaWaitHandleSource *) (s))

typedef struct _FridaWaitHandleSource FridaWaitHandleSource;

struct _FridaWaitHandleSource
{
  GSource source;

  HANDLE handle;
  gboolean owns_handle;
  GPollFD handle_poll_fd;
};

static void frida_wait_handle_source_finalize (GSource * source);

static gboolean frida_wait_handle_source_prepare (GSource * source,
    gint * timeout);
static gboolean frida_wait_handle_source_check (GSource * source);
static gboolean frida_wait_handle_source_dispatch (GSource * source,
    GSourceFunc callback, gpointer user_data);

static GSourceFuncs frida_wait_handle_source_funcs = {
  frida_wait_handle_source_prepare,
  frida_wait_handle_source_check,
  frida_wait_handle_source_dispatch,
  frida_wait_handle_source_finalize
};

GSource *
frida_wait_handle_source_create (void * handle, gboolean owns_handle)
{
  GSource * source;
  GPollFD * pfd;
  FridaWaitHandleSource * whsrc;

  source = g_source_new (&frida_wait_handle_source_funcs,
      sizeof (FridaWaitHandleSource));
  whsrc = FRIDA_WAIT_HANDLE_SOURCE (source);
  whsrc->handle = handle;
  whsrc->owns_handle = owns_handle;

  pfd = &FRIDA_WAIT_HANDLE_SOURCE (source)->handle_poll_fd;
#if GLIB_SIZEOF_VOID_P == 8
  pfd->fd = (gint64) handle;
#else
  pfd->fd = (gint) handle;
#endif
  pfd->events = G_IO_IN | G_IO_OUT | G_IO_HUP | G_IO_ERR;
  pfd->revents = 0;
  g_source_add_poll (source, pfd);

  return source;
}

static void
frida_wait_handle_source_finalize (GSource * source)
{
  FridaWaitHandleSource * self = FRIDA_WAIT_HANDLE_SOURCE (source);

  if (self->owns_handle)
    CloseHandle (self->handle);
}

static gboolean
frida_wait_handle_source_prepare (GSource * source, gint * timeout)
{
  FridaWaitHandleSource * self = FRIDA_WAIT_HANDLE_SOURCE (source);

  *timeout = -1;

  return WaitForSingleObject (self->handle, 0) == WAIT_OBJECT_0;
}

static gboolean
frida_wait_handle_source_check (GSource * source)
{
  FridaWaitHandleSource * self = FRIDA_WAIT_HANDLE_SOURCE (source);

  return WaitForSingleObject (self->handle, 0) == WAIT_OBJECT_0;
}

static gboolean
frida_wait_handle_source_dispatch (GSource * source, GSourceFunc callback,
    gpointer user_data)
{
  g_assert (WaitForSingleObject (FRIDA_WAIT_HANDLE_SOURCE (source)->handle, 0) == WAIT_OBJECT_0);

  return callback (user_data);
}
