#pragma once

#using <PresentationCore.dll>
#using <WindowsBase.dll>

#include <frida-core.h>
#include <msclr/gcroot.h>

using namespace System;
using namespace System::Windows::Media;
using System::Windows::Threading::Dispatcher;

namespace Frida
{
  ref class Process;
  ref class Session;

  public enum class DeviceType
  {
    Local,
    Remote,
    Usb
  };

  public enum class Scope
  {
    Minimal,
    Metadata,
    Full
  };

  public ref class Device
  {
  internal:
    Device (FridaDevice * handle, Dispatcher ^ dispatcher);
  public:
    ~Device ();
  protected:
    !Device ();

  public:
    event EventHandler ^ Lost;

    property String ^ Id { String ^ get (); }
    property String ^ Name { String ^ get (); }
    property ImageSource ^ Icon { ImageSource ^ get (); }
    property DeviceType Type { DeviceType get (); }

    array<Process ^> ^ EnumerateProcesses ();
    array<Process ^> ^ EnumerateProcesses (Scope scope);
    array<Process ^> ^ EnumerateProcesses (array<unsigned int> ^ pids, Scope scope);
    unsigned int Spawn (String ^ program, array<String ^> ^ argv, array<String ^> ^ envp, array<String ^> ^ env, String ^ cwd);
    void Resume (unsigned int pid);
    Session ^ Attach (unsigned int pid);

    virtual String ^ ToString () override;

  internal:
    void OnLost (Object ^ sender, EventArgs ^ e);

  private:
    FridaDevice * handle;
    msclr::gcroot<Device ^> * selfHandle;

    Dispatcher ^ dispatcher;
    ImageSource ^ icon;
    EventHandler ^ onLostHandler;
  };
}
