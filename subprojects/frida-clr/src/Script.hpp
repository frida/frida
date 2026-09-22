#pragma once

#using <WindowsBase.dll>

#include <frida-core.h>
#include <msclr/gcroot.h>

using namespace System;
using System::Windows::Threading::Dispatcher;

namespace Frida
{
  ref class ScriptMessageEventArgs;
  public delegate void ScriptMessageHandler (Object ^ sender, ScriptMessageEventArgs ^ e);

  public ref class Script
  {
  internal:
    Script (FridaScript * handle, Dispatcher ^ dispatcher);
  public:
    ~Script ();
  protected:
    !Script ();

  public:
    event ScriptMessageHandler ^ Message;

    void Load ();
    void Unload ();
    void Eternalize ();
    void Post (String ^ message);
    void PostWithData (String ^ message, array<unsigned char> ^ data);
    void EnableDebugger ();
    void EnableDebugger (UInt16 port);
    void DisableDebugger ();

  internal:
    void OnMessage (Object ^ sender, ScriptMessageEventArgs ^ e);

  private:
    FridaScript * handle;
    msclr::gcroot<Script ^> * selfHandle;

    Dispatcher ^ dispatcher;
    ScriptMessageHandler ^ onMessageHandler;
  };

  public ref class ScriptMessageEventArgs : public EventArgs
  {
  public:
    property String ^ Message { String ^ get () { return message; } };
    property array<unsigned char> ^ Data { array<unsigned char> ^ get () { return data; } };

    ScriptMessageEventArgs (String ^ message, array<unsigned char> ^ data)
    {
      this->message = message;
      this->data = data;
    }

  private:
    String ^ message;
    array<unsigned char> ^ data;
  };
}
