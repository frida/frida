#include "frida-helper-backend.h"

#if defined (HAVE_IOS) || defined (HAVE_TVOS) || defined (HAVE_XROS)
# include "policyd.h"
# include "substituted-client.h"
# include "substituted2-client.h"

# include <errno.h>
# include <mach/mach.h>

# ifndef TASK_SEATBELT_PORT
# define TASK_SEATBELT_PORT 7
# endif

typedef int (* JbdCallFunc) (mach_port_t service_port, guint command, guint pid);

extern kern_return_t bootstrap_look_up (mach_port_t bootstrap_port, char * service_name, mach_port_t * service_port);

void
_frida_internal_iostvos_policy_softener_soften (guint pid,
                                                GError ** error)
{
  static mach_port_t service_port = MACH_PORT_NULL;
  kern_return_t kr;
  int error_code;

  if (service_port == MACH_PORT_NULL)
  {
    kr = bootstrap_look_up (bootstrap_port, FRIDA_POLICYD_SERVICE_NAME, &service_port);
    if (kr != KERN_SUCCESS)
      goto service_not_available;
  }

  kr = frida_policyd_soften (service_port, pid, &error_code);
  if (kr != KERN_SUCCESS)
    goto service_crashed;

  if (error_code != 0)
    goto softening_failed;

  return;

service_not_available:
  {
    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Policy daemon is not running");

    return;
  }
service_crashed:
  {
    mach_port_deallocate (mach_task_self (), service_port);
    service_port = MACH_PORT_NULL;

    g_set_error (error,
        FRIDA_ERROR,
        FRIDA_ERROR_NOT_SUPPORTED,
        "Policy daemon has crashed");

    return;
  }
softening_failed:
  {
    if (error_code == ESRCH)
    {
      g_set_error (error,
          FRIDA_ERROR,
          FRIDA_ERROR_PROCESS_NOT_FOUND,
          "No such process");
    }
    else
    {
      g_set_error (error,
          FRIDA_ERROR,
          FRIDA_ERROR_PERMISSION_DENIED,
          "%s while attempting to soften target process",
          g_strerror (error_code));
    }

    return;
  }
}

guint
_frida_electra_policy_softener_internal_jb_connect (void)
{
  mach_port_t service_port = MACH_PORT_NULL;
  kern_return_t kr;

  kr = bootstrap_look_up (bootstrap_port, "org.coolstar.jailbreakd", &service_port);
  if (kr != KERN_SUCCESS)
    return MACH_PORT_NULL;

  return service_port;
}

void
_frida_electra_policy_softener_internal_jb_disconnect (guint service_port)
{
  mach_port_deallocate (mach_task_self (), service_port);
}

gint
_frida_electra_policy_softener_internal_jb_entitle_now (void * jbd_call, guint service_port, guint pid)
{
  JbdCallFunc jbd_call_func = jbd_call;

  return jbd_call_func (service_port, 1, pid);
}

guint
_frida_unc0ver_policy_softener_internal_connect (void)
{
  mach_port_t service_port = MACH_PORT_NULL;
  kern_return_t kr;

  kr = task_get_special_port (mach_task_self (), TASK_SEATBELT_PORT, &service_port);
  if (kr != KERN_SUCCESS)
    return MACH_PORT_NULL;

  return service_port;
}

void
_frida_unc0ver_policy_softener_internal_disconnect (guint service_port)
{
  mach_port_deallocate (mach_task_self (), service_port);
}

void
_frida_unc0ver_policy_softener_internal_substitute_setup_process (guint service_port, guint pid)
{
  kern_return_t kr;
  static gboolean using_new_substituted = FALSE;

  if (service_port == MACH_PORT_NULL)
    return;

  /*
   * DISCLAIMER:
   * Don't do this at home. This is not recommended outside of the
   * Frida use case and may change in the future. Instead, just
   * drop your stuff in /Library/MobileSubstrate/DynamicLibraries
   */

  if (!using_new_substituted)
  {
    kr = substitute_setup_process (service_port, pid, FALSE, FALSE);
    if (kr == MIG_BAD_ARGUMENTS)
      using_new_substituted = TRUE;
  }

  if (using_new_substituted)
    kr = substitute2_setup_process (service_port, pid, FALSE, FALSE);

  if (kr != KERN_SUCCESS)
    g_warning ("substitute_setup_process() failed for PID %u", pid);
}

#endif
