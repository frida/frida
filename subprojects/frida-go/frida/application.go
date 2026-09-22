package frida

//#include <frida-core.h>
import "C"

import (
	"fmt"
	"unsafe"
)

// Application represents the main application installed on the device
type Application struct {
	application *C.FridaApplication
}

// Identifier returns application bundle identifier
func (a *Application) Identifier() string {
	return C.GoString(C.frida_application_get_identifier(a.application))
}

// Name returns application name
func (a *Application) Name() string {
	return C.GoString(C.frida_application_get_name(a.application))
}

// PID returns application PID or "-" if it could not be obtained when application is not running
func (a *Application) PID() int {
	return int(C.frida_application_get_pid(a.application))
}

// String returns the string representation of Application printing identifier, name and pid
func (a *Application) String() string {
	return fmt.Sprintf("Identifier: %s Name: %s PID: %d", a.Identifier(), a.Name(), a.PID())
}

// Params return the application parameters, like version, path etc
func (a *Application) Params() map[string]any {
	ht := C.frida_application_get_parameters(a.application)
	params := gHashTableToMap(ht)
	return params
}

// Clean will clean resources held by the application.
func (a *Application) Clean() {
	clean(unsafe.Pointer(a.application), unrefFrida)
}
