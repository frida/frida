package frida

//#include <frida-core.h>
import "C"
import "unsafe"

// SessionOptions type is used to configure session
type SessionOptions struct {
	opts *C.FridaSessionOptions
}

// NewSessionOptions create new SessionOptions with the realm and
// timeout to persist provided
func NewSessionOptions(realm Realm, persistTimeout uint) *SessionOptions {
	opts := C.frida_session_options_new()
	C.frida_session_options_set_realm(opts, C.FridaRealm(realm))
	C.frida_session_options_set_persist_timeout(opts, C.guint(persistTimeout))

	return &SessionOptions{opts}
}

// Realm returns the realm of the options
func (s *SessionOptions) Realm() Realm {
	rlm := C.frida_session_options_get_realm(s.opts)
	return Realm(rlm)
}

// PersistTimeout returns the persist timeout of the script.s
func (s *SessionOptions) PersistTimeout() int {
	return int(C.frida_session_options_get_persist_timeout(s.opts))
}

// Clean will clean the resources held by the session options.
func (s *SessionOptions) Clean() {
	clean(unsafe.Pointer(s.opts), unrefFrida)
}
