package frida

/*#include <frida-core.h>
#include <stdlib.h>
*/
import "C"
import "unsafe"

// RemoteDeviceOptions type is used to configure the remote device.
type RemoteDeviceOptions struct {
	opts *C.FridaRemoteDeviceOptions
}

// NewRemoteDeviceOptions returns the new remote device options.
func NewRemoteDeviceOptions() *RemoteDeviceOptions {
	opts := C.frida_remote_device_options_new()

	return &RemoteDeviceOptions{
		opts: opts,
	}
}

// Certificate returns the certificate for the remote device options.
func (r *RemoteDeviceOptions) Certificate() *Certificate {
	cert := C.frida_remote_device_options_get_certificate(r.opts)
	return &Certificate{cert}
}

// Origin returns the origin for the remote device options.
func (r *RemoteDeviceOptions) Origin() string {
	return C.GoString(C.frida_remote_device_options_get_origin(r.opts))
}

// Token returns the token for the remote device options.
func (r *RemoteDeviceOptions) Token() string {
	return C.GoString(C.frida_remote_device_options_get_token(r.opts))
}

// KeepAliveInterval returns the keepalive interval for the remote device options.
func (r *RemoteDeviceOptions) KeepAliveInterval() int {
	return int(C.frida_remote_device_options_get_keepalive_interval(r.opts))
}

// SetCertificate sets the certificate for the remote device.
func (r *RemoteDeviceOptions) SetCertificate(certPath string) error {
	cert, err := gTLSCertificateFromFile(certPath)
	if err != nil {
		return err
	}

	C.frida_remote_device_options_set_certificate(r.opts, cert.cert)
	return nil
}

// SetOrigin sets the origin for the remote device options.
func (r *RemoteDeviceOptions) SetOrigin(origin string) {
	originC := C.CString(origin)
	defer C.free(unsafe.Pointer(originC))

	C.frida_remote_device_options_set_origin(r.opts, originC)
}

// SetToken sets the token for the remote device options.
func (r *RemoteDeviceOptions) SetToken(token string) {
	tokenC := C.CString(token)
	defer C.free(unsafe.Pointer(tokenC))

	C.frida_remote_device_options_set_token(r.opts, tokenC)
}

// SetKeepAlive sets keepalive interval for the remote device options.
func (r *RemoteDeviceOptions) SetKeepAlive(interval int) {
	C.frida_remote_device_options_set_keepalive_interval(r.opts, C.gint(interval))
}

// Clean will clean the resources held by the remote device options.
func (r *RemoteDeviceOptions) Clean() {
	clean(unsafe.Pointer(r.opts), unrefFrida)
}

func gTLSCertificateFromFile(pempath string) (*Certificate, error) {
	cert := C.CString(pempath)
	defer C.free(unsafe.Pointer(cert))

	var err *C.GError
	gTLSCert := C.g_tls_certificate_new_from_file(cert, &err)
	if err != nil {
		return nil, &FError{err}
	}

	return &Certificate{gTLSCert}, nil
}

func gFileFromPath(assetPath string) *C.GFile {
	pth := C.CString(assetPath)
	defer C.free(unsafe.Pointer(pth))

	return C.g_file_new_for_path(pth)
}
