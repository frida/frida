#include "Session.hpp"

#include "Marshal.hpp"
#include "Runtime.hpp"
#include "Script.hpp"

using System::Windows::Threading::DispatcherPriority;

namespace Frida
{
  static void OnSessionDetached (FridaSession * session, FridaSessionDetachReason reason, FridaCrash * crash, gpointer user_data);

  Session::Session (FridaSession * handle, Dispatcher ^ dispatcher)
    : handle (handle),
      dispatcher (dispatcher)
  {
    Runtime::Ref ();

    selfHandle = new msclr::gcroot<Session ^> (this);
    onDetachedHandler = gcnew SessionDetachedHandler (this, &Session::OnDetached);
    g_signal_connect (handle, "detached", G_CALLBACK (OnSessionDetached), selfHandle);
  }

  Session::~Session ()
  {
    if (handle == NULL)
      return;

    g_signal_handlers_disconnect_by_func (handle, OnSessionDetached, selfHandle);
    delete selfHandle;
    selfHandle = NULL;

    this->!Session ();
  }

  Session::!Session ()
  {
    if (handle != NULL)
    {
      g_object_unref (handle);
      handle = NULL;

      Runtime::Unref ();
    }
  }

  unsigned int
  Session::Pid::get ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Session");
    return frida_session_get_pid (handle);
  }

  void
  Session::Detach ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Session");
    frida_session_detach_sync (handle, nullptr, nullptr);
  }

  Script ^
  Session::CreateScript (String ^ source)
  {
    return CreateScript (source, nullptr);
  }

  Script ^
  Session::CreateScript (String ^ source, String ^ name)
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Session");

    gchar * sourceUtf8 = Marshal::ClrStringToUTF8CString (source);

    FridaScriptOptions * options = frida_script_options_new ();

    if (name != nullptr)
    {
      gchar * nameUtf8 = Marshal::ClrStringToUTF8CString (name);
      frida_script_options_set_name (options, nameUtf8);
      g_free (nameUtf8);
    }

    GError * error = NULL;
    FridaScript * script = frida_session_create_script_sync (handle, sourceUtf8, options, nullptr, &error);

    g_object_unref (options);

    g_free (sourceUtf8);

    Marshal::ThrowGErrorIfSet (&error);

    return gcnew Script (script, dispatcher);
  }

  void
  Session::OnDetached (Object ^ sender, SessionDetachedEventArgs ^ e)
  {
    if (dispatcher->CheckAccess ())
      Detached (sender, e);
    else
      dispatcher->BeginInvoke (DispatcherPriority::Normal, onDetachedHandler, sender, e);
  }

  static void
  OnSessionDetached (FridaSession * session, FridaSessionDetachReason reason, FridaCrash * crash, gpointer user_data)
  {
    (void) session;

    msclr::gcroot<Session ^> * wrapper = static_cast<msclr::gcroot<Session ^> *> (user_data);
    SessionDetachedEventArgs ^ e = gcnew SessionDetachedEventArgs (static_cast<SessionDetachReason> (reason));
    (*wrapper)->OnDetached (*wrapper, e);
  }
}
