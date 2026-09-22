package frida

//#include <frida-core.h>
import "C"
import "fmt"

const (
	defaultDeviceTimeout  = 10
	defaultProcessTimeout = 10
)

type DeviceType int

const (
	DeviceTypeLocal DeviceType = iota
	DeviceTypeRemote
	DeviceTypeUsb
)

func (d DeviceType) String() string {
	return [...]string{"local",
		"remote",
		"usb"}[d]
}

type Realm int

const (
	RealmNative Realm = iota
	RealmEmulated
)

func (r Realm) String() string {
	return [...]string{"native",
		"emulated"}[r]
}

type ScriptRuntime int

const (
	ScriptRuntimeDefault ScriptRuntime = iota
	ScriptRuntimeQJS
	ScriptRuntimeV8
)

func (s ScriptRuntime) String() string {
	return [...]string{"default",
		"qjs",
		"v8"}[s]
}

type Scope int

const (
	ScopeMinimal Scope = iota
	ScopeMetadata
	ScopeFull
)

func (s Scope) String() string {
	return [...]string{"minimal",
		"metadata",
		"full"}[s]
}

type Stdio int

const (
	StdioInherit Stdio = iota
	StdioPipe
)

func (s Stdio) String() string {
	return [...]string{"inherit",
		"pipe"}[s]
}

type Runtime int

const (
	RuntimeDefault Runtime = iota
	RuntimeQJS
	RuntimeV8
)

func (r Runtime) String() string {
	return [...]string{"default",
		"qjs",
		"v8"}[r]
}

type ChildOrigin int

const (
	ChildOriginFork ChildOrigin = iota
	ChildOriginExec
	ChildOriginSpawn
)

func (origin ChildOrigin) String() string {
	return [...]string{"fork",
		"exec",
		"spawn"}[origin]
}

type RelayKind int

const (
	RelayKindTurnUDP RelayKind = iota
	RelayKindTurnTCP
	RelayKindTurnTLS
)

func (kind RelayKind) String() string {
	return [...]string{"turn-udp",
		"turn-tcp",
		"turn-tls"}[kind]
}

type SessionDetachReason int

const (
	SessionDetachReasonApplicationRequested SessionDetachReason = iota + 1
	SessionDetachReasonProcessReplaced
	SessionDetachReasonProcessTerminated
	SessionDetachReasonServerTerminated
	SessionDetachReasonDeviceLost
)

func (reason SessionDetachReason) String() string {
	return [...]string{"",
		"application-requested",
		"process-replaced",
		"process-terminated",
		"server-terminated",
		"device-list"}[reason]
}

type SnapshotTransport int

const (
	SnapshotTransportInline SnapshotTransport = iota
	SnapshotTransportSharedMemory
)

// Address represents structure returned by some specific signals.
type Address struct {
	Addr string
	Port uint16
}

// String representation of Address in format ADDR:PORT
func (a *Address) String() string {
	return fmt.Sprintf("%s:%d", a.Addr, a.Port)
}
