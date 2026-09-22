package frida

//#include <frida-core.h>
import "C"

import "unsafe"

// FileMonitor type is the type responsible for monitoring file changes
type FileMonitor struct {
	fm *C.FridaFileMonitor
}

// NewFileMonitor creates new FileMonito with the file path provided.
func NewFileMonitor(path string) *FileMonitor {
	pathC := C.CString(path)
	defer C.free(unsafe.Pointer(pathC))

	m := C.frida_file_monitor_new(pathC)

	return &FileMonitor{
		fm: m,
	}
}

// Path returns the path of the monitored file.
func (mon *FileMonitor) Path() string {
	return C.GoString(C.frida_file_monitor_get_path(mon.fm))
}

// Enable enables file monitoring.
func (mon *FileMonitor) Enable() error {
	var err *C.GError
	C.frida_file_monitor_enable_sync(mon.fm, nil, &err)
	if err != nil {
		return &FError{err}
	}
	return nil
}

// Disable disables file monitoring.
func (mon *FileMonitor) Disable() error {
	var err *C.GError
	C.frida_file_monitor_disable_sync(mon.fm, nil, &err)
	if err != nil {
		return &FError{err}
	}
	return nil
}

// Clean will clean the resources held by the file monitor.
func (mon *FileMonitor) Clean() {
	clean(unsafe.Pointer(mon.fm), unrefFrida)
}

// On connects file monitor to specific signals. Once sigName is triggered,
// fn callback will be called with parameters populated.
//
// Signals available are:
//   - "change" with callback as func(changedFile, otherFile, changeType string) {}
func (mon *FileMonitor) On(sigName string, fn any) {
	connectClosure(unsafe.Pointer(mon.fm), sigName, fn)
}
