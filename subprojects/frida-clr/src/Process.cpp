#include "Process.hpp"

#include "Marshal.hpp"
#include "Runtime.hpp"

namespace Frida
{
  Process::Process (FridaProcess * handle)
    : handle (handle),
      parameters (nullptr),
      icons (nullptr)
  {
    Runtime::Ref ();
  }

  Process::~Process ()
  {
    if (handle == NULL)
      return;

    delete icons;
    icons = nullptr;

    delete parameters;
    parameters = nullptr;

    this->!Process ();
  }

  Process::!Process ()
  {
    if (handle != NULL)
    {
      g_object_unref (handle);
      handle = NULL;

      Runtime::Unref ();
    }
  }

  unsigned int
  Process::Pid::get ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Process");
    return frida_process_get_pid (handle);
  }

  String ^
  Process::Name::get ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Process");
    return Marshal::UTF8CStringToClrString (frida_process_get_name (handle));
  }

  IDictionary<String ^, Object ^> ^
  Process::Parameters::get ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Process");
    if (parameters == nullptr)
      parameters = Marshal::ParametersDictToClrDictionary (frida_process_get_parameters (handle));
    return parameters;
  }

  array<ImageSource ^> ^
  Process::Icons::get ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Process");
    if (icons == nullptr)
    {
      Object ^ val;
      if (Parameters->TryGetValue ("icons", val))
        icons = Marshal::IconArrayToClrImageSourceArray (val);
      else
        icons = gcnew array<ImageSource ^> (0);
    }
    return icons;
  }

  String ^
  Process::ToString ()
  {
    if (handle == NULL)
      throw gcnew ObjectDisposedException ("Process");
    return String::Format ("Pid: {0}, Name: \"{1}\"", Pid, Name);
  }
}