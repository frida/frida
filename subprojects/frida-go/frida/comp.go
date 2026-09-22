package frida

/*#include <frida-core.h>
 */
import "C"
import (
	"reflect"
	"unsafe"
)

// Compiler type is used to compile scripts.
type Compiler struct {
	cc *C.FridaCompiler
	fn reflect.Value
}

// NewCompiler creates new compiler.
func NewCompiler() *Compiler {
	mgr := getDeviceManager()
	cc := C.frida_compiler_new(mgr.getManager())

	return &Compiler{
		cc: cc,
	}
}

// Build builds the script from the entrypoint.
func (c *Compiler) Build(entrypoint string) (string, error) {
	entrypointC := C.CString(entrypoint)
	defer C.free(unsafe.Pointer(entrypointC))

	var err *C.GError
	ret := C.frida_compiler_build_sync(c.cc, entrypointC, nil, nil, &err)
	if err != nil {
		return "", &FError{err}
	}

	return C.GoString(ret), nil
}

// Watch watches for changes at the entrypoint and sends the "output" signal.
func (c *Compiler) Watch(entrypoint string) error {
	entrypointC := C.CString(entrypoint)
	defer C.free(unsafe.Pointer(entrypointC))

	var err *C.GError
	C.frida_compiler_watch_sync(c.cc, entrypointC, nil, nil, &err)
	if err != nil {
		return &FError{err}
	}

	return nil
}

// Clean will clean resources held by the compiler.
func (c *Compiler) Clean() {
	clean(unsafe.Pointer(c.cc), unrefFrida)
}

// On connects compiler to specific signals. Once sigName is triggered,
// fn callback will be called with parameters populated.
//
// Signals available are:
//   - "starting" with callback as func() {}
//   - "finished" with callback as func() {}
//   - "output" with callback as func(bundle string) {}
//   - "diagnostics" with callback as func(diag string) {}
//   - "file_changed" with callback as func() {}
func (c *Compiler) On(sigName string, fn any) {
	// hijack diagnostics and pass only text
	if sigName == "diagnostics" {
		c.fn = reflect.ValueOf(fn)
		connectClosure(unsafe.Pointer(c.cc), sigName, c.hijackFn)
	} else {
		connectClosure(unsafe.Pointer(c.cc), sigName, fn)
	}
}

func (c *Compiler) hijackFn(diag map[string]any) {
	text := diag["text"].(string)
	args := []reflect.Value{reflect.ValueOf(text)}
	c.fn.Call(args)
}
