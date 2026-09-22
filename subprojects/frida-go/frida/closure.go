package frida

/*
#include <frida-core.h>

extern void deleteClosure(gpointer, GClosure*);
extern void goMarshalCls(GClosure*, GValue*, guint, GValue*, gpointer, GValue*);

static GClosure * newClosure() {
	GClosure * closure = g_closure_new_simple(sizeof(GClosure), NULL);
	g_closure_set_marshal(closure, (GClosureMarshal)(goMarshalCls));
	g_closure_add_finalize_notifier(closure, NULL, (GClosureNotify)(deleteClosure));

	return closure;
}

static GType getVType(GValue * val) {
	return (G_VALUE_TYPE(val));
}

static guint lookup_signal(void * obj, char * sigName) {
	return g_signal_lookup(sigName, G_OBJECT_TYPE(obj));
}
*/
import "C"
import (
	"fmt"
	"reflect"
	"runtime"
	"sync"
	"unsafe"
)

var closures = &sync.Map{}

//export deleteClosure
func deleteClosure(ptr C.gpointer, closure *C.GClosure) {
	closures.Delete(unsafe.Pointer(closure))
}

//export goMarshalCls
func goMarshalCls(gclosure *C.GClosure, returnValue *C.GValue, nParams C.guint,
	params *C.GValue,
	invocationHint C.gpointer,
	marshalData *C.GValue) {

	var closure funcstack
	cV, ok := closures.Load(unsafe.Pointer(gclosure))
	if !ok {
		closure = funcstack{}
	} else {
		closure = cV.(funcstack)
	}

	countOfParams := int(nParams)

	fnType := closure.Func.Type()
	fnCountArgs := fnType.NumIn()

	if fnCountArgs > countOfParams {
		msg := fmt.Sprintf("too many args: have %d, max %d\n", fnCountArgs, countOfParams)
		panic(msg)
	}

	gvalues := func(params *C.GValue, count int) []C.GValue {
		var slc []C.GValue
		hdr := (*reflect.SliceHeader)(unsafe.Pointer(&slc))
		hdr.Cap = count
		hdr.Len = count
		hdr.Data = uintptr(unsafe.Pointer(params))

		return slc
	}(params, countOfParams)

	fnArgs := make([]reflect.Value, fnCountArgs)

	for i := 0; i < fnCountArgs; i++ {
		goV := getGoValueFromGValue(&gvalues[i+1])
		fnArgs[i] = reflect.ValueOf(goV).Convert(fnType.In(i))
	}

	closure.Func.Call(fnArgs)
}

type funcstack struct {
	Func   reflect.Value
	Frames []uintptr
}

func connectClosure(obj unsafe.Pointer, sigName string, fn any) {
	v := reflect.ValueOf(fn)

	if v.Type().Kind() != reflect.Func {
		panic("got no function")
	}

	frames := make([]uintptr, 3)
	frames = frames[:runtime.Callers(2+2, frames)]

	fs := funcstack{
		Func:   v,
		Frames: frames,
	}

	sigC := C.CString(sigName)
	defer C.free(unsafe.Pointer(sigC))

	gclosure := newClosureFunc(fs)
	sigID := C.lookup_signal(obj, sigC)

	// Do nothing if signal is 0 meaning not found
	if int(sigID) != 0 {
		C.g_signal_connect_closure_by_id((C.gpointer)(obj), sigID, 0, gclosure, C.gboolean(1))
	}
}

func newClosureFunc(fnStack funcstack) *C.GClosure {
	cls := C.newClosure()
	closures.Store(unsafe.Pointer(cls), fnStack)
	return cls
}
