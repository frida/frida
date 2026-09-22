package frida

//#include <frida-core.h>
import "C"
import (
	"errors"
	"reflect"
	"runtime"
	"sort"
	"unsafe"
)

type DeviceInt interface {
	ID() string
	Name() string
	DeviceIcon() any
	Bus() *Bus
	Manager() *DeviceManager
	IsLost() bool
	Params() (map[string]any, error)
	FrontmostApplication(scope Scope) (*Application, error)
	EnumerateApplications(identifier string, scope Scope) ([]*Application, error)
	ProcessByPID(pid int, scope Scope) (*Process, error)
	ProcessByName(name string, scope Scope) (*Process, error)
	FindProcessByPID(pid int, scope Scope) (*Process, error)
	FindProcessByName(name string, scope Scope) (*Process, error)
	EnumerateProcesses(scope Scope) ([]*Process, error)
	EnableSpawnGating() error
	DisableSpawnGating() error
	EnumeratePendingSpawn() ([]*Spawn, error)
	EnumeratePendingChildren() ([]*Child, error)
	Spawn(name string, opts *SpawnOptions) (int, error)
	Input(pid int, data []byte) error
	Resume(pid int) error
	Kill(pid int) error
	Attach(val any, opts *SessionOptions) (*Session, error)
	InjectLibraryFile(target any, path, entrypoint, data string) (uint, error)
	InjectLibraryBlob(target any, byteData []byte, entrypoint, data string) (uint, error)
	OpenChannel(address string) (*IOStream, error)
	Clean()
	On(sigName string, fn any)
}

// Device represents Device struct from frida-core
type Device struct {
	device *C.FridaDevice
}

// ID will return the ID of the device.
func (d *Device) ID() string {
	if d.device != nil {
		return C.GoString(C.frida_device_get_id(d.device))
	}
	return ""
}

// Name will return the name of the device.
func (d *Device) Name() string {
	if d.device != nil {
		return C.GoString(C.frida_device_get_name(d.device))
	}
	return ""
}

// DeviceIcon will return the device icon.
func (d *Device) DeviceIcon() any {
	if d.device != nil {
		icon := C.frida_device_get_icon(d.device)
		dt := gPointerToGo((C.gpointer)(icon))
		return dt
	}
	return nil
}

// DeviceType returns type of the device.
func (d *Device) DeviceType() DeviceType {
	if d.device != nil {
		fdt := C.frida_device_get_dtype(d.device)
		return DeviceType(fdt)
	}
	return -1
}

// Bus returns device bus.
func (d *Device) Bus() *Bus {
	if d.device != nil {
		bus := C.frida_device_get_bus(d.device)
		return &Bus{
			bus: bus,
		}
	}
	return nil
}

// Manager returns device manager for the device.
func (d *Device) Manager() *DeviceManager {
	if d.device != nil {
		mgr := C.frida_device_get_manager(d.device)
		return &DeviceManager{mgr}
	}
	return nil
}

// IsLost returns boolean whether device is lost or not.
func (d *Device) IsLost() bool {
	if d.device != nil {
		lost := C.frida_device_is_lost(d.device)
		return int(lost) == 1
	}
	return false
}

// Params returns system parameters of the device
func (d *Device) Params() (map[string]any, error) {
	if d.device != nil {
		var err *C.GError
		ht := C.frida_device_query_system_parameters_sync(d.device, nil, &err)
		if err != nil {
			return nil, &FError{err}
		}

		params := gHashTableToMap(ht)

		return params, nil
	}
	return nil, errors.New("could not obtain params for nil device")
}

// FrontmostApplication will return the frontmost application or the application in focus
// on the device.
func (d *Device) FrontmostApplication(scope Scope) (*Application, error) {
	if d.device != nil {
		var err *C.GError
		app := &Application{}

		sc := C.FridaScope(scope)
		queryOpts := C.frida_frontmost_query_options_new()
		C.frida_frontmost_query_options_set_scope(queryOpts, sc)
		app.application = C.frida_device_get_frontmost_application_sync(d.device,
			queryOpts,
			nil,
			&err)
		if err != nil {
			return nil, &FError{err}
		}

		if app.application == nil {
			return nil, errors.New("could not obtain frontmost application! Is any application started?")
		}

		return app, nil
	}
	return nil, errors.New("could not obtain frontmost app for nil device")
}

// EnumerateApplications will return slice of applications on the device
func (d *Device) EnumerateApplications(identifier string, scope Scope) ([]*Application, error) {
	if d.device != nil {
		queryOpts := C.frida_application_query_options_new()
		C.frida_application_query_options_set_scope(queryOpts, C.FridaScope(scope))

		if identifier != "" {
			identifierC := C.CString(identifier)
			defer C.free(unsafe.Pointer(identifierC))
			C.frida_application_query_options_select_identifier(queryOpts, identifierC)
		}

		var err *C.GError
		appList := C.frida_device_enumerate_applications_sync(d.device, queryOpts, nil, &err)
		if err != nil {
			return nil, &FError{err}
		}

		appListSize := int(C.frida_application_list_size(appList))
		apps := make([]*Application, appListSize)

		for i := 0; i < appListSize; i++ {
			app := C.frida_application_list_get(appList, C.gint(i))
			apps[i] = &Application{app}
		}

		sort.Slice(apps, func(i, j int) bool {
			return apps[i].PID() > apps[j].PID()
		})

		clean(unsafe.Pointer(queryOpts), unrefFrida)
		clean(unsafe.Pointer(appList), unrefFrida)

		return apps, nil
	}
	return nil, errors.New("could not enumerate applications for nil device")
}

// ProcessByPID returns the process by passed pid.
func (d *Device) ProcessByPID(pid int, scope Scope) (*Process, error) {
	if d.device != nil {
		opts := C.frida_process_match_options_new()
		C.frida_process_match_options_set_timeout(opts, C.gint(defaultProcessTimeout))
		C.frida_process_match_options_set_scope(opts, C.FridaScope(scope))
		defer clean(unsafe.Pointer(opts), unrefFrida)

		var err *C.GError
		proc := C.frida_device_get_process_by_pid_sync(d.device, C.guint(pid), opts, nil, &err)
		if err != nil {
			return nil, &FError{err}
		}
		return &Process{proc}, nil
	}
	return nil, errors.New("could not obtain process for nil device")
}

// ProcessByName returns the process by passed name.
func (d *Device) ProcessByName(name string, scope Scope) (*Process, error) {
	if d.device != nil {
		nameC := C.CString(name)
		defer C.free(unsafe.Pointer(nameC))

		opts := C.frida_process_match_options_new()
		C.frida_process_match_options_set_timeout(opts, C.gint(defaultProcessTimeout))
		C.frida_process_match_options_set_scope(opts, C.FridaScope(scope))
		defer clean(unsafe.Pointer(opts), unrefFrida)

		var err *C.GError
		proc := C.frida_device_get_process_by_name_sync(d.device, nameC, opts, nil, &err)
		if err != nil {
			return nil, &FError{err}
		}
		return &Process{proc}, nil
	}
	return nil, errors.New("could not obtain process for nil device")
}

// FindProcessByPID will try to find the process with given pid.
func (d *Device) FindProcessByPID(pid int, scope Scope) (*Process, error) {
	if d.device != nil {
		opts := C.frida_process_match_options_new()
		C.frida_process_match_options_set_timeout(opts, C.gint(defaultProcessTimeout))
		C.frida_process_match_options_set_scope(opts, C.FridaScope(scope))
		defer clean(unsafe.Pointer(opts), unrefFrida)

		var err *C.GError
		proc := C.frida_device_find_process_by_pid_sync(d.device, C.guint(pid), opts, nil, &err)
		if err != nil {
			return nil, &FError{err}
		}
		return &Process{proc}, nil
	}
	return nil, errors.New("could not find process for nil device")
}

// FindProcessByName will try to find the process with name specified.
func (d *Device) FindProcessByName(name string, scope Scope) (*Process, error) {
	if d.device != nil {
		nameC := C.CString(name)
		defer C.free(unsafe.Pointer(nameC))

		opts := C.frida_process_match_options_new()
		C.frida_process_match_options_set_timeout(opts, C.gint(defaultProcessTimeout))
		C.frida_process_match_options_set_scope(opts, C.FridaScope(scope))
		defer clean(unsafe.Pointer(opts), unrefFrida)

		var err *C.GError
		proc := C.frida_device_find_process_by_name_sync(d.device, nameC, opts, nil, &err)
		if err != nil {
			return nil, &FError{err}
		}
		return &Process{proc}, nil
	}
	return nil, errors.New("could not find process for nil device")
}

// EnumerateProcesses will slice of processes running with scope provided
func (d *Device) EnumerateProcesses(scope Scope) ([]*Process, error) {
	if d.device != nil {
		opts := C.frida_process_query_options_new()
		C.frida_process_query_options_set_scope(opts, C.FridaScope(scope))
		defer clean(unsafe.Pointer(opts), unrefFrida)

		var err *C.GError
		procList := C.frida_device_enumerate_processes_sync(d.device, opts, nil, &err)
		if err != nil {
			return nil, &FError{err}
		}

		procListSize := int(C.frida_process_list_size(procList))
		procs := make([]*Process, procListSize)

		for i := 0; i < procListSize; i++ {
			proc := C.frida_process_list_get(procList, C.gint(i))
			procs[i] = &Process{proc}
		}

		clean(unsafe.Pointer(procList), unrefFrida)
		return procs, nil
	}
	return nil, errors.New("could not enumerate processes for nil device")
}

// EnableSpawnGating will enable spawn gating on the device.
func (d *Device) EnableSpawnGating() error {
	if d.device != nil {
		var err *C.GError
		C.frida_device_enable_spawn_gating_sync(d.device, nil, &err)
		if err != nil {
			return &FError{err}
		}
		return nil
	}
	return errors.New("could not enable spawn gating for nil device")
}

// DisableSpawnGating will disable spawn gating on the device.
func (d *Device) DisableSpawnGating() error {
	if d.device != nil {
		var err *C.GError
		C.frida_device_disable_spawn_gating_sync(d.device, nil, &err)
		if err != nil {
			return &FError{err}
		}
		return nil
	}
	return errors.New("could not disable spawn gating for nil device")
}

// EnumeratePendingSpawn will return the slice of pending spawns.
func (d *Device) EnumeratePendingSpawn() ([]*Spawn, error) {
	if d.device != nil {
		var err *C.GError
		spawnList := C.frida_device_enumerate_pending_spawn_sync(d.device, nil, &err)
		if err != nil {
			return nil, &FError{err}
		}

		spawnListSize := int(C.frida_spawn_list_size(spawnList))
		spawns := make([]*Spawn, spawnListSize)

		for i := 0; i < spawnListSize; i++ {
			spawn := C.frida_spawn_list_get(spawnList, C.gint(i))
			spawns[i] = &Spawn{spawn}
		}

		clean(unsafe.Pointer(spawnList), unrefFrida)
		return spawns, nil
	}
	return nil, errors.New("could not enumerate pending spawn for nil device")
}

// EnumeratePendingChildren will return the slice of pending children.
func (d *Device) EnumeratePendingChildren() ([]*Child, error) {
	if d.device != nil {
		var err *C.GError
		childList := C.frida_device_enumerate_pending_children_sync(d.device, nil, &err)
		if err != nil {
			return nil, &FError{err}
		}

		childListSize := int(C.frida_child_list_size(childList))
		children := make([]*Child, childListSize)

		for i := 0; i < childListSize; i++ {
			child := C.frida_child_list_get(childList, C.gint(i))
			children[i] = &Child{child}
		}

		clean(unsafe.Pointer(childList), unrefFrida)
		return children, nil
	}
	return nil, errors.New("could not enumerate pending children for nil device")
}

// Spawn will spawn an application or binary.
func (d *Device) Spawn(name string, opts *SpawnOptions) (int, error) {
	if d.device != nil {
		var opt *C.FridaSpawnOptions = nil
		if opts != nil {
			opt = opts.opts
		}
		defer clean(unsafe.Pointer(opt), unrefFrida)

		nameC := C.CString(name)
		defer C.free(unsafe.Pointer(nameC))

		var err *C.GError
		pid := C.frida_device_spawn_sync(d.device, nameC, opt, nil, &err)
		if err != nil {
			return -1, &FError{err}
		}

		return int(pid), nil
	}
	return -1, errors.New("could not spawn for nil device")
}

// Input inputs []bytes into the process with pid specified.
func (d *Device) Input(pid int, data []byte) error {
	if d.device != nil {
		gBytesData := goBytesToGBytes(data)
		runtime.SetFinalizer(gBytesData, func(g *C.GBytes) {
			clean(unsafe.Pointer(g), unrefGObject)
		})

		var err *C.GError
		C.frida_device_input_sync(d.device, C.guint(pid), gBytesData, nil, &err)
		runtime.KeepAlive(gBytesData)
		if err != nil {
			return &FError{err}
		}
		return nil
	}
	return errors.New("could not input bytes into nil device")
}

// Resume will resume the process with pid.
func (d *Device) Resume(pid int) error {
	if d.device != nil {
		var err *C.GError
		C.frida_device_resume_sync(d.device, C.guint(pid), nil, &err)
		if err != nil {
			return &FError{err}
		}
		return nil
	}
	return errors.New("could not resume for nil device")
}

// Kill kills process with pid specified.
func (d *Device) Kill(pid int) error {
	if d.device != nil {
		var err *C.GError
		C.frida_device_kill_sync(d.device, C.guint(pid), nil, &err)
		if err != nil {
			return &FError{err}
		}
		return nil
	}
	return errors.New("could not kill for nil device")
}

// Attach will attach on specified process name or PID.
// You can pass the nil as SessionOptions or you can create it if you want
// the session to persist for specific timeout.
func (d *Device) Attach(val any, opts *SessionOptions) (*Session, error) {
	if d.device != nil {
		var pid int
		switch v := reflect.ValueOf(val); v.Kind() {
		case reflect.String:
			proc, err := d.ProcessByName(val.(string), ScopeMinimal)
			if err != nil {
				return nil, err
			}
			pid = proc.PID()
		case reflect.Int:
			pid = val.(int)
		default:
			return nil, errors.New("expected name of app/process or PID")
		}

		var opt *C.FridaSessionOptions = nil
		if opts != nil {
			opt = opts.opts
			defer clean(unsafe.Pointer(opt), unrefFrida)
		}

		var err *C.GError
		s := C.frida_device_attach_sync(d.device, C.guint(pid), opt, nil, &err)
		if err != nil {
			return nil, &FError{err}
		}
		return &Session{s}, nil
	}
	return nil, errors.New("could not attach for nil device")
}

// InjectLibraryFile will inject the library in the target with path to library specified.
// Entrypoint is the entrypoint to the library and the data is any data you need to pass
// to the library.
func (d *Device) InjectLibraryFile(target any, path, entrypoint, data string) (uint, error) {
	if d.device != nil {
		var pid int
		switch v := reflect.ValueOf(target); v.Kind() {
		case reflect.String:
			proc, err := d.ProcessByName(target.(string), ScopeMinimal)
			if err != nil {
				return 0, err
			}
			pid = proc.PID()
		case reflect.Int:
			pid = target.(int)
		default:
			return 0, errors.New("expected name of app/process or PID")
		}

		if path == "" {
			return 0, errors.New("you need to provide path to library")
		}

		var pathC *C.char
		var entrypointC *C.char = nil
		var dataC *C.char = nil

		pathC = C.CString(path)
		defer C.free(unsafe.Pointer(pathC))

		if entrypoint != "" {
			entrypointC = C.CString(entrypoint)
			defer C.free(unsafe.Pointer(entrypointC))
		}

		if data != "" {
			dataC = C.CString(data)
			defer C.free(unsafe.Pointer(dataC))
		}

		var err *C.GError
		id := C.frida_device_inject_library_file_sync(d.device,
			C.guint(pid),
			pathC,
			entrypointC,
			dataC,
			nil,
			&err)
		if err != nil {
			return 0, &FError{err}
		}

		return uint(id), nil
	}
	return 0, errors.New("could not inject library for nil device")
}

// InjectLibraryBlob will inject the library in the target with byteData path.
// Entrypoint is the entrypoint to the library and the data is any data you need to pass
// to the library.
func (d *Device) InjectLibraryBlob(target any, byteData []byte, entrypoint, data string) (uint, error) {
	if d.device != nil {
		var pid int
		switch v := reflect.ValueOf(target); v.Kind() {
		case reflect.String:
			proc, err := d.ProcessByName(target.(string), ScopeMinimal)
			if err != nil {
				return 0, err
			}
			pid = proc.PID()
		case reflect.Int:
			pid = target.(int)
		default:
			return 0, errors.New("expected name of app/process or PID")
		}

		if len(byteData) == 0 {
			return 0, errors.New("you need to provide byteData")
		}

		var entrypointC *C.char = nil
		var dataC *C.char = nil

		if entrypoint != "" {
			entrypointC = C.CString(entrypoint)
			defer C.free(unsafe.Pointer(entrypointC))
		}

		if data != "" {
			dataC = C.CString(data)
			defer C.free(unsafe.Pointer(dataC))
		}

		gBytesData := goBytesToGBytes(byteData)
		runtime.SetFinalizer(gBytesData, func(g *C.GBytes) {
			defer clean(unsafe.Pointer(g), unrefGObject)
		})

		var err *C.GError
		id := C.frida_device_inject_library_blob_sync(d.device,
			C.guint(pid),
			gBytesData,
			entrypointC,
			dataC,
			nil,
			&err)
		runtime.KeepAlive(gBytesData)
		if err != nil {
			return 0, &FError{err}
		}

		return uint(id), nil
	}
	return 0, errors.New("could not inject library blob for nil device")
}

// OpenChannel open channel with the address and returns the IOStream
func (d *Device) OpenChannel(address string) (*IOStream, error) {
	if d.device != nil {
		addressC := C.CString(address)
		defer C.free(unsafe.Pointer(addressC))

		var err *C.GError
		stream := C.frida_device_open_channel_sync(d.device, addressC, nil, &err)
		if err != nil {
			return nil, &FError{err}
		}
		return NewIOStream(stream), nil
	}
	return nil, errors.New("could not open channel for nil device")
}

// Clean will clean the resources held by the device.
func (d *Device) Clean() {
	if d.device != nil {
		clean(unsafe.Pointer(d.device), unrefFrida)
	}
}

// On connects device to specific signals. Once sigName is triggered,
// fn callback will be called with parameters populated.
//
// Signals available are:
//   - "spawn_added" with callback as func(spawn *frida.Spawn) {}
//   - "spawn_removed" with callback as func(spawn *frida.Spawn) {}
//   - "child_added" with callback as func(child *frida.Child) {}
//   - "child_removed" with callback as func(child *frida.Child) {}
//   - "process_crashed" with callback as func(crash *frida.Crash) {}
//   - "output" with callback as func(pid, fd int, data []byte) {}
//   - "uninjected" with callback as func(id int) {}
//   - "lost" with callback as func() {}
func (d *Device) On(sigName string, fn any) {
	if d.device != nil {
		connectClosure(unsafe.Pointer(d.device), sigName, fn)
	}
}
