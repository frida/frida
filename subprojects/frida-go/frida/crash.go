package frida

//#include <frida-core.h>
import "C"
import (
	"fmt"
	"unsafe"
)

// Crash represents crash of frida.
type Crash struct {
	crash *C.FridaCrash
}

// PID returns the process identifier oc.crashed application
func (c *Crash) PID() int {
	if c.crash != nil {
		return int(C.frida_crash_get_pid(c.crash))
	}
	return -1
}

// ProcessName returns the name of the process that crashed
func (c *Crash) ProcessName() string {
	if c.crash != nil {
		return C.GoString(C.frida_crash_get_process_name(c.crash))
	}
	return ""
}

// Summary returns the summary of the crash
func (c *Crash) Summary() string {
	if c.crash != nil {
		return C.GoString(C.frida_crash_get_summary(c.crash))
	}
	return ""
}

// Report returns the report of the crash
func (c *Crash) Report() string {
	if c.crash != nil {
		return C.GoString(C.frida_crash_get_report(c.crash))
	}
	return ""
}

// Params returns the parameters of the crash.
func (c *Crash) Params() map[string]any {
	if c.crash != nil {
		ht := C.frida_crash_get_parameters(c.crash)
		params := gHashTableToMap(ht)
		return params
	}
	return nil
}

// String returns string interpretation of the crash
func (c *Crash) String() string {
	if c.crash != nil {
		return fmt.Sprintf("<FridaCrash>: <%p>", c.crash)
	}
	return ""
}

// Clean will clean resources held by the crash.
func (c *Crash) Clean() {
	if c.crash != nil {
		clean(unsafe.Pointer(c.crash), unrefFrida)
	}
}
