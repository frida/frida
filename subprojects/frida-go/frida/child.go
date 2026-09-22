package frida

//#include <frida-core.h>
import "C"
import "unsafe"

// Child type represents child when child gating is enabled.
type Child struct {
	child *C.FridaChild
}

// PID returns the process id of the child.
func (f *Child) PID() uint {
	return uint(C.frida_child_get_pid(f.child))
}

// PPID returns the parent process id of the child.
func (f *Child) PPID() uint {
	return uint(C.frida_child_get_parent_pid(f.child))
}

// Origin returns the origin of the child.
func (f *Child) Origin() ChildOrigin {
	return ChildOrigin(C.frida_child_get_origin(f.child))
}

// Identifier returns string identifier of the child.
func (f *Child) Identifier() string {
	return C.GoString(C.frida_child_get_identifier(f.child))
}

// Path returns the path of the child.
func (f *Child) Path() string {
	return C.GoString(C.frida_child_get_path(f.child))
}

// Argv returns argv passed to the child.
func (f *Child) Argv() []string {
	var length C.gint
	arr := C.frida_child_get_argv(f.child, &length)

	return cArrayToStringSlice(arr, C.int(length))
}

// Envp returns envp passed to the child.
func (f *Child) Envp() []string {
	var length C.gint
	arr := C.frida_child_get_envp(f.child, &length)

	return cArrayToStringSlice(arr, C.int(length))
}

// Clean will clean resources held by the child.
func (f *Child) Clean() {
	clean(unsafe.Pointer(f.child), unrefFrida)
}
