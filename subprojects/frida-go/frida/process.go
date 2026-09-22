package frida

//#include <frida-core.h>
import "C"
import "unsafe"

// Process represents process on the device.
type Process struct {
	proc *C.FridaProcess
}

// PID returns the PID of the process.
func (p *Process) PID() int {
	if p.proc != nil {
		return int(C.frida_process_get_pid(p.proc))
	}
	return -1
}

// Name returns the name of the process.
func (p *Process) Name() string {
	if p.proc != nil {
		return C.GoString(C.frida_process_get_name(p.proc))
	}
	return ""
}

// Params returns the parameters of the process.
func (p *Process) Params() map[string]any {
	if p.proc != nil {
		ht := C.frida_process_get_parameters(p.proc)
		params := gHashTableToMap(ht)
		return params
	}
	return nil
}

// Clean will clean the resources held by the process.
func (p *Process) Clean() {
	if p.proc != nil {
		clean(unsafe.Pointer(p.proc), unrefFrida)
	}
}
