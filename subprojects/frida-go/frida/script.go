package frida

//#include <frida-core.h>
import "C"
import (
	"context"
	"encoding/json"
	"reflect"
	"runtime"
	"strings"
	"sync"
	"unsafe"

	"github.com/google/uuid"
)

var rpcCalls = sync.Map{}

// Script represents loaded string in the memory.
type Script struct {
	hasHandler bool
	sc         *C.FridaScript
	fn         reflect.Value
}

// IsDestroyed function returns whether the script previously loaded is destroyed (could be caused by unload)
func (s *Script) IsDestroyed() bool {
	destroyed := C.frida_script_is_destroyed(s.sc)
	return int(destroyed) == 1
}

// Load function loads the script into the process.
func (s *Script) Load() error {
	if !s.hasHandler {
		s.On("message", func() {})
	}
	var err *C.GError
	C.frida_script_load_sync(s.sc, nil, &err)
	if err != nil {
		return &FError{err}
	}
	return nil
}

// Unload function unload previously loaded script
func (s *Script) Unload() error {
	var err *C.GError
	C.frida_script_unload_sync(s.sc, nil, &err)
	if err != nil {
		return &FError{err}
	}
	return nil
}

// Eternalize function will keep the script loaded even after deataching from the process
func (s *Script) Eternalize() error {
	var err *C.GError
	C.frida_script_eternalize_sync(s.sc, nil, &err)
	if err != nil {
		return &FError{err}
	}
	return nil
}

// Post sends post to the script.
func (s *Script) Post(jsonString string, data []byte) {
	jsonStringC := C.CString(jsonString)
	defer C.free(unsafe.Pointer(jsonStringC))

	if len(data) > 0 {
		gBytesData := goBytesToGBytes(data)

		runtime.SetFinalizer(gBytesData, func(g *C.GBytes) {
			clean(unsafe.Pointer(g), unrefGObject)
		})
		C.frida_script_post(s.sc, jsonStringC, gBytesData)
		runtime.KeepAlive(gBytesData)
	} else {
		C.frida_script_post(s.sc, jsonStringC, nil)
	}
}

// EnableDebugger function enables debugging on the port specified
func (s *Script) EnableDebugger(port uint16) error {
	var err *C.GError
	C.frida_script_enable_debugger_sync(s.sc, C.guint16(port), nil, &err)
	if err != nil {
		return &FError{err}
	}

	return nil
}

// DisableDebugger function disables debugging
func (s *Script) DisableDebugger() error {
	var err *C.GError
	C.frida_script_disable_debugger_sync(s.sc, nil, &err)
	if err != nil {
		return &FError{err}
	}
	return nil
}

// ExportsCall will try to call fn from the rpc.exports with args provided
func (s *Script) ExportsCall(fn string, args ...any) any {
	ch := s.makeExportsCall(fn, args...)
	ret := <-ch
	return ret
}

// ExportsCallWithContext will try to call fn from the rpc.exports with args provided using context provided.
func (s *Script) ExportsCallWithContext(ctx context.Context, fn string, args ...any) any {
	ch := s.makeExportsCall(fn, args...)

	for {
		select {
		case <-ctx.Done():
			// because the context is done, we still need to read from the channel
			go func() {
				<-ch
			}()
			return ErrContextCancelled
		case ret := <-ch:
			return ret
		}
	}
}

// Clean will clean the resources held by the script.
func (s *Script) Clean() {
	clean(unsafe.Pointer(s.sc), unrefFrida)
}

// On connects script to specific signals. Once sigName is triggered,
// fn callback will be called with parameters populated.
//
// Signals available are:
//   - "destroyed" with callback as func() {}
//   - "message" with callback as func(message string, data []byte) {}
func (s *Script) On(sigName string, fn any) {
	s.hasHandler = true
	// hijack message to handle rpc calls
	if sigName == "message" {
		s.fn = reflect.ValueOf(fn)
		connectClosure(unsafe.Pointer(s.sc), sigName, s.hijackFn)
	} else {
		connectClosure(unsafe.Pointer(s.sc), sigName, fn)
	}
}

func getRPCIDFromMessage(message string) (string, any, error) {
	unmarshalled := make(map[string]any)
	if err := json.Unmarshal([]byte(message), &unmarshalled); err != nil {
		return "", nil, err
	}

	var rpcID string
	var ret any

	loopMap := func(mp map[string]any) {
		for _, v := range mp {
			if reflect.ValueOf(v).Kind() == reflect.Slice {
				slc := v.([]any)
				rpcID = slc[1].(string)
				ret = slc[3]

			}
		}
	}
	loopMap(unmarshalled)

	return rpcID, ret, nil
}

func (s *Script) hijackFn(message string, data []byte) {
	if strings.Contains(message, "frida:rpc") {
		rpcID, ret, err := getRPCIDFromMessage(message)
		if err != nil {
			panic(err)
		}
		callerCh, ok := rpcCalls.Load(rpcID)
		if !ok {
			panic("rpc-id not found")
		}
		ch := callerCh.(chan any)
		ch <- ret
		close(ch)

	} else {
		var args []reflect.Value
		switch s.fn.Type().NumIn() {
		case 1:
			args = append(args, reflect.ValueOf(message))
		case 2:
			args = append(args, reflect.ValueOf(message))
			args = append(args, reflect.ValueOf(data))
		}
		s.fn.Call(args)
	}
}

func newRPCCall(fnName string) []any {
	id := uuid.New()
	dt := []any{
		"frida:rpc",
		id.String()[:16],
		"call",
		fnName,
	}

	return dt
}

func (s *Script) makeExportsCall(fn string, args ...any) chan any {
	rpcData := newRPCCall(fn)

	aIface := make([]any, len(args))
	copy(aIface, args)

	ct := 0
	rpc := make([]any, len(rpcData)+len(aIface))

	for i := 0; i < len(rpcData); i++ {
		rpc[ct] = rpcData[i]
		ct++
	}

	if len(aIface) > 0 {
		rpc[ct] = aIface
	}

	ch := make(chan any)
	rpcCalls.Store(rpcData[1], ch)

	bt, _ := json.Marshal(rpc)
	s.Post(string(bt), nil)

	return ch
}
