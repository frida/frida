package frida

//#include <frida-core.h>
import "C"
import "unsafe"

// PortalOptions type represents struct used to connect to the portal.
type PortalOptions struct {
	opts *C.FridaPortalOptions
}

// NewPortalOptions creates new portal options.
func NewPortalOptions() *PortalOptions {
	opts := C.frida_portal_options_new()
	return &PortalOptions{
		opts: opts,
	}
}

// Certificate returns the tls certificate for portal options.
func (p *PortalOptions) Certificate() *Certificate {
	cert := C.frida_portal_options_get_certificate(p.opts)
	return &Certificate{cert}
}

// Token returns the token for the portal.
func (p *PortalOptions) Token() string {
	return C.GoString(C.frida_portal_options_get_token(p.opts))
}

// ACL returns the acls for the portal.
func (p *PortalOptions) ACL() []string {
	var sz C.gint
	arr := C.frida_portal_options_get_acl(p.opts, &sz)
	return cArrayToStringSlice(arr, C.int(sz))
}

// SetCertificate sets the certificate for the portal.
func (p *PortalOptions) SetCertificate(certPath string) error {
	cert, err := gTLSCertificateFromFile(certPath)
	if err != nil {
		return err
	}

	C.frida_portal_options_set_certificate(p.opts, cert.cert)
	return nil
}

// SetToken sets the token for the authentication.
func (p *PortalOptions) SetToken(token string) {
	tokenC := C.CString(token)
	defer C.free(unsafe.Pointer(tokenC))

	C.frida_portal_options_set_token(p.opts, tokenC)
}

// SetACL sets the acls from the string slice provided.
func (p *PortalOptions) SetACL(acls []string) {
	arr, sz := stringSliceToCharArr(acls)
	C.frida_portal_options_set_acl(p.opts, arr, C.gint(sz))
	freeCharArray(arr, C.int(sz))
}

// Clean will clean the resources held by the portal options.
func (p *PortalOptions) Clean() {
	clean(unsafe.Pointer(p.opts), unrefFrida)
}
