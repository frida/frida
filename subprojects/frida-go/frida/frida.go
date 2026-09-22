// Package frida provides Go bindings for frida.
// Some of the provided functionality includes:
//
// * Listing devices/applications/processes
// * Attaching to applications/processes
// * Fetching information about devices/applications/processes
package frida

/*
#cgo LDFLAGS: -lfrida-core -lm -ldl
#cgo CFLAGS: -I/usr/local/include/ -w
#cgo darwin LDFLAGS: -lbsm -framework Foundation -framework AppKit -lresolv -lpthread
#cgo darwin CFLAGS: -Wno-error=incompatible-function-pointer-types
#cgo android LDFLAGS: -llog
#cgo android CFLAGS: -DANDROID -Wno-error=incompatible-function-pointer-types
#cgo linux,!android LDFLAGS: -lrt -lresolv -lpthread
#cgo linux CFLAGS: -pthread
#include <frida-core.h>
#include "android-selinux.h"
*/
import "C"
import (
	"sync"
)

var data = &sync.Map{}

// PatchAndroidSELinux tries to patch selinux; root access is required.
func PatchAndroidSELinux() {
	C.android_patch_selinux()
}

// Version returns currently used frida version
func Version() string {
	return C.GoString(C.frida_version_string())
}

func getDeviceManager() *DeviceManager {
	v, ok := data.Load("mgr")
	if !ok {
		mgr := NewDeviceManager()
		data.Store("mgr", mgr)
		return mgr
	}
	return v.(*DeviceManager)
}

// LocalDevice is a wrapper around DeviceByType(DeviceTypeLocal).
func LocalDevice() *Device {
	mgr := getDeviceManager()
	v, ok := data.Load("localDevice")
	if !ok {
		dev, _ := mgr.DeviceByType(DeviceTypeLocal)
		data.Store("localDevice", dev)
		return dev.(*Device)
	}
	return v.(*Device)
}

// USBDevice is a wrapper around DeviceByType(DeviceTypeUsb).
func USBDevice() *Device {
	mgr := getDeviceManager()
	v, ok := data.Load("usbDevice")
	if !ok {
		_, ok := data.Load("enumeratedDevices")
		if !ok {
			mgr.EnumerateDevices()
			data.Store("enumeratedDevices", true)
		}
		dev, err := mgr.DeviceByType(DeviceTypeUsb)
		if err != nil {
			return nil
		}
		data.Store("usbDevice", dev)
		return dev.(*Device)
	}
	return v.(*Device)
}

// DeviceByID tries to get the device by id on the default manager
func DeviceByID(id string) (*Device, error) {
	mgr := getDeviceManager()
	v, ok := data.Load(id)
	if !ok {
		_, ok := data.Load("enumeratedDevices")
		if !ok {
			mgr.EnumerateDevices()
			data.Store("enumeratedDevices", true)
		}
		dev, err := mgr.DeviceByID(id)
		if err != nil {
			return nil, err
		}
		data.Store(id, dev)
		return v.(*Device), nil
	}
	return v.(*Device), nil
}

// Attach attaches at val(string or int pid) using local device.
func Attach(val any) (*Session, error) {
	dev := LocalDevice()
	return dev.Attach(val, nil)
}
