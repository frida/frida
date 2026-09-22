#include "Script.hpp"

#include "Marshal.hpp"
#include "Runtime.hpp"

using System::Windows::Threading::DispatcherPriority;

namespace Frida
{
  static void OnScriptMessage (FridaScript * script, const gchar * message, GBytes * data, gpointer user_data);

  Script::Script (FridaScript * handle, Dispatcher ^ dispatcher)
    : handle (handle),
      dispatcher (dispatcher)
  {
    Runtime::Ref ();

    selfHandle = new msclr::gcroot<Script ^> (this);
    onMessageHandler = gcnew ScriptMessageHandler (this, &Script::OnMessage);
    g_signal_connect (handle, "message", G_CALLBACK (OnScriptMessage), selfHandle);
  }

  Script::~Script ()
  {
    if (handle == NULL)
      return;

    g_signal_handlers_disconnect_by_func (handle, OnScriptMessage, selfHandle);
    delete selfHandle;
    selfHandle = NULL;

    this->!Script ();
  }

  Script::!Script ()
  {
    if (handle != NULL)
    {
      g_object_unref (handle);
      handle = NULL;

      Runtime::Unref ();
    }
  }

  void
  Script::Load ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Script");

    GError * error = NULL;
    frida_script_load_sync (handle, nullptr, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Script::Unload ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Script");

    GError * error = NULL;
    frida_script_unload_sync (handle, nullptr, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Script::Eternalize ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Script");

    GError * error = NULL;
    frida_script_eternalize_sync (handle, nullptr, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Script::Post (String ^ message)
  {
    PostWithData (message, nullptr);
  }

  void
  Script::PostWithData (String ^ message, array<unsigned char> ^ data)
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Script");

    gchar * messageUtf8 = Marshal::ClrStringToUTF8CString (message);
    GBytes * dataBytes = Marshal::ClrByteArrayToBytes (data);
    frida_script_post (handle, messageUtf8, dataBytes);
    g_bytes_unref (dataBytes);
    g_free (messageUtf8);
  }

  void
  Script::EnableDebugger ()
  {
    return EnableDebugger (0);
  }

  void
  Script::EnableDebugger (UInt16 port)
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Script");

    GError * error = NULL;
    frida_script_enable_debugger_sync (handle, port, nullptr, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Script::DisableDebugger ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Script");

    GError * error = NULL;
    frida_script_disable_debugger_sync (handle, nullptr, &error);
    Marshal::ThrowGErrorIfSet (&error);
  }

  void
  Script::OnMessage (Object ^ sender, ScriptMessageEventArgs ^ e)
  {
    if (dispatcher->CheckAccess ())
      Message (sender, e);
    else
      dispatcher->BeginInvoke (DispatcherPriority::Normal, onMessageHandler, sender, e);
  }

  static void
  OnScriptMessage (FridaScript * script, const gchar * message, GBytes * data, gpointer user_data)
  {
    (void) script;

    msclr::gcroot<Script ^> * wrapper = static_cast<msclr::gcroot<Script ^> *> (user_data);
    ScriptMessageEventArgs ^ e = gcnew ScriptMessageEventArgs (
        Marshal::UTF8CStringToClrString (message),
        Marshal::BytesToClrArray (data));
   (*wrapper)->OnMessage (*wrapper, e);
  }
}
