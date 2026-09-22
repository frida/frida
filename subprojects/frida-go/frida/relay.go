package frida

//#include <frida-core.h>
import "C"
import "unsafe"

// Relay type represents relay for setting up p2p.
type Relay struct {
	r *C.FridaRelay
}

// NewRelay creates the new relay with the credentials provided.
func NewRelay(address, username, password string, kind RelayKind) *Relay {
	var addressC *C.char = nil
	var usernameC *C.char = nil
	var passwordC *C.char = nil

	if address != "" {
		addressC = C.CString(address)
		defer C.free(unsafe.Pointer(addressC))
	}

	if username != "" {
		usernameC = C.CString(username)
		defer C.free(unsafe.Pointer(usernameC))
	}

	if password != "" {
		passwordC = C.CString(password)
		defer C.free(unsafe.Pointer(passwordC))
	}

	knd := C.FridaRelayKind(kind)

	rly := C.frida_relay_new(
		addressC,
		usernameC,
		passwordC,
		knd)

	return &Relay{rly}
}

// Address returns the address of the relay.
func (relay *Relay) Address() string {
	return C.GoString(C.frida_relay_get_address(relay.r))
}

// Username returns the username for the relay.
func (relay *Relay) Username() string {
	return C.GoString(C.frida_relay_get_username(relay.r))
}

// Password returns the password for the relay.
func (relay *Relay) Password() string {
	return C.GoString(C.frida_relay_get_password(relay.r))
}

// RelayKind returns the kind of relay.
func (relay *Relay) RelayKind() RelayKind {
	return RelayKind(C.frida_relay_get_kind(relay.r))
}

// Clean will clean the resources held by the relay.
func (relay *Relay) Clean() {
	clean(unsafe.Pointer(relay.r), unrefFrida)
}
