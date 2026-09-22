package frida

//#include <frida-core.h>
import "C"
import (
	"runtime"
	"unsafe"
)

// Session type represents the session with the device.
type Session struct {
	s *C.FridaSession
}

// IsDetached returns bool whether session is detached or not.
func (s *Session) IsDetached() bool {
	detached := C.frida_session_is_detached(s.s)
	return int(detached) == 1
}

// Detach detaches the current session.
func (s *Session) Detach() error {
	var err *C.GError
	C.frida_session_detach_sync(s.s, nil, &err)
	if err != nil {
		return &FError{err}
	}
	return nil
}

// Resume resumes the current session.
func (s *Session) Resume() error {
	var err *C.GError
	C.frida_session_resume_sync(s.s, nil, &err)
	if err != nil {
		return &FError{err}
	}
	return nil
}

// EnableChildGating enables child gating on the session.
func (s *Session) EnableChildGating() error {
	var err *C.GError
	C.frida_session_enable_child_gating_sync(s.s, nil, &err)
	if err != nil {
		return &FError{err}
	}

	return nil
}

// DisableChildGating disables child gating on the session.
func (s *Session) DisableChildGating() error {
	var err *C.GError
	C.frida_session_disable_child_gating_sync(s.s, nil, &err)
	if err != nil {
		return &FError{err}
	}

	return nil
}

// CreateScript creates new string from the string provided.
func (s *Session) CreateScript(script string) (*Script, error) {
	return s.CreateScriptWithOptions(script, nil)
}

// CreateScriptBytes is a wrapper around CreateScript(script string)
func (s *Session) CreateScriptBytes(script []byte, opts *ScriptOptions) (*Script, error) {
	bts := goBytesToGBytes(script)
	runtime.SetFinalizer(bts, func(g *C.GBytes) {
		clean(unsafe.Pointer(g), unrefGObject)
	})

	if opts == nil {
		opts = NewScriptOptions("frida-go")
	}
	defer clean(unsafe.Pointer(opts.opts), unrefFrida)

	var err *C.GError
	sc := C.frida_session_create_script_from_bytes_sync(s.s,
		bts,
		opts.opts,
		nil,
		&err)
	runtime.KeepAlive(bts)
	if err != nil {
		return nil, &FError{err}
	}

	return &Script{
		sc: sc,
	}, nil
}

func (s *Session) CreateScriptWithSnapshot(script string, snapshot []byte) (*Script, error) {
	opts := NewScriptOptions("frida-go")
	opts.SetSnapshot(snapshot)
	return s.CreateScriptWithOptions(script, opts)
}

// CreateScriptWithOptions creates the script with the script options provided.
// Useful in cases where you previously created the snapshot.
func (s *Session) CreateScriptWithOptions(script string, opts *ScriptOptions) (*Script, error) {
	sc := C.CString(script)
	defer C.free(unsafe.Pointer(sc))

	if opts == nil {
		opts = NewScriptOptions("frida-go")
	}
	defer clean(unsafe.Pointer(opts.opts), unrefFrida)

	if opts.Name() == "" {
		opts.SetName("frida-go")
	}

	var err *C.GError
	cScript := C.frida_session_create_script_sync(s.s, sc, opts.opts, nil, &err)
	if err != nil {
		return nil, &FError{err}
	}
	return &Script{
		sc: cScript,
	}, nil
}

// CompileScript compiles the script from the script as string provided.
func (s *Session) CompileScript(script string, opts *ScriptOptions) ([]byte, error) {
	scriptC := C.CString(script)
	defer C.free(unsafe.Pointer(scriptC))

	if opts == nil {
		opts = NewScriptOptions("frida-go")
	}
	defer clean(unsafe.Pointer(opts.opts), unrefFrida)

	var err *C.GError
	bts := C.frida_session_compile_script_sync(s.s,
		scriptC,
		opts.opts,
		nil,
		&err)
	if err != nil {
		return nil, &FError{err}
	}

	return getGBytes(bts), nil
}

// SnapshotScript creates snapshot from the script.
func (s *Session) SnapshotScript(embedScript string, snapshotOpts *SnapshotOptions) ([]byte, error) {
	embedScriptC := C.CString(embedScript)
	defer C.free(unsafe.Pointer(embedScriptC))

	var err *C.GError
	ret := C.frida_session_snapshot_script_sync(
		s.s,
		embedScriptC,
		snapshotOpts.opts,
		nil,
		&err)

	if err != nil {
		return nil, &FError{err}
	}

	bts := getGBytes(ret)

	return bts, nil
}

// SetupPeerConnection sets up peer (p2p) connection with peer options provided.
func (s *Session) SetupPeerConnection(opts *PeerOptions) error {
	var err *C.GError
	C.frida_session_setup_peer_connection_sync(s.s, opts.opts, nil, &err)
	if err != nil {
		return &FError{err}
	}
	return nil
}

// JoinPortal joins portal at the address with portal options provided.
func (s *Session) JoinPortal(address string, opts *PortalOptions) (*PortalMembership, error) {
	addrC := C.CString(address)
	defer C.free(unsafe.Pointer(addrC))

	var err *C.GError
	mem := C.frida_session_join_portal_sync(s.s, addrC, opts.opts, nil, &err)
	if err != nil {
		return nil, &FError{err}
	}

	return &PortalMembership{mem}, nil
}

// Clean will clean the resources held by the session.
func (s *Session) Clean() {
	clean(unsafe.Pointer(s.s), unrefFrida)
}

// On connects session to specific signals. Once sigName is triggered,
// fn callback will be called with parameters populated.
//
// Signals available are:
//   - "detached" with callback as func(reason frida.SessionDetachReason, crash *frida.Crash) {}
func (s *Session) On(sigName string, fn any) {
	connectClosure(unsafe.Pointer(s.s), sigName, fn)
}
