package frida

//#include <frida-core.h>
import "C"
import (
	"unsafe"
)

type cleanupType string

const (
	unrefGError  cleanupType = "*GError"
	unrefFrida   cleanupType = "frida types"
	unrefGObject cleanupType = "GObject*"
)

type cleanupFn func(unsafe.Pointer)

var cleanups = map[cleanupType]cleanupFn{
	unrefGError:  gErrorFree,
	unrefFrida:   unrefGObj,
	unrefGObject: unrefGObj,
}

func gErrorFree(err unsafe.Pointer) {
	C.g_error_free((*C.GError)(err))
}

func unrefGObj(obj unsafe.Pointer) {
	C.g_object_unref((C.gpointer)(obj))
}

func clean(obj unsafe.Pointer, cType cleanupType) {
	if obj != nil {
		fn := cleanups[cType]
		if fn != nil {
			fn(obj)
		}
	}
}

func freeCharArray(arr **C.char, size C.int) {
	for i := 0; i < int(size); i++ {
		elem := getCharArrayElement(arr, i)
		C.free(unsafe.Pointer(elem))
	}
	C.free(unsafe.Pointer(arr))
}
