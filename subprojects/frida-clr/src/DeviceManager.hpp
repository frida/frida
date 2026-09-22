#pragma once

#using <WindowsBase.dll>

#include <frida-core.h>
#include <msclr/gcroot.h>

using namespace System;
using System::Windows::Threading::Dispatcher;

namespace Frida
{
  ref class Device;

  public ref class DeviceManager
  {
  public:
    DeviceManager (Dispatcher ^ dispatcher);
    ~DeviceManager ();
  protected:
    !DeviceManager ();

  public:
    event EventHandler ^ Changed;

    array<Device ^> ^ EnumerateDevices ();

  internal:
    void OnChanged (Object ^ sender, EventArgs ^ e);

  private:
    FridaDeviceManager * handle;
    msclr::gcroot<DeviceManager ^> * selfHandle;

    Dispatcher ^ dispatcher;
    EventHandler ^ onChangedHandler;
  };
}