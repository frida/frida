package frida

//#include "authentication-service.h"
import "C"
import (
	"errors"
	"unsafe"
)

// AuthenticationFn is a callback function passed to the endpoint params.
// Function does authentication and in the case the user is authenticated,
// non-empty string is returned.
// If the user is not authenticated, empty string should be returned.
type AuthenticationFn func(string) string

// EParams represent config needed to setup endpoint parameters that are used to setup Portal.
// Types of authentication includes:
//   - no authentication (not providing Token nor AuthenticationCallback)
//   - static authentication (providing token)
//   - authentication using callback (providing AuthenticationCallback)
//
// If the Token and AuthenticationCallback are passed, static authentication will be used (token based)
type EParams struct {
	Address                string
	Port                   uint16
	Certificate            string
	Origin                 string
	Token                  string
	AuthenticationCallback AuthenticationFn
	AssetRoot              string
}

// EndpointParameters represent internal FridaEndpointParameters
type EndpointParameters struct {
	params *C.FridaEndpointParameters
}

//export authenticate
func authenticate(cb unsafe.Pointer, token *C.char) *C.char {
	fn := (*func(string) string)(cb)
	ret := (*fn)(C.GoString(token))
	if ret == "" {
		return nil
	}
	return C.CString(ret)
}

// NewEndpointParameters returns *EndpointParameters needed to setup Portal by using
// provided EParams object.
func NewEndpointParameters(params *EParams) (*EndpointParameters, error) {
	if params.Address == "" {
		return nil, errors.New("you need to provide address")
	}

	addrC := C.CString(params.Address)
	defer C.free(unsafe.Pointer(addrC))

	var tknC *C.char = nil
	var originC *C.char = nil
	var authService *C.FridaAuthenticationService = nil
	var assetPath *C.GFile = nil

	if params.Token != "" {
		tknC = C.CString(params.Token)
		defer C.free(unsafe.Pointer(tknC))

		authService = (*C.FridaAuthenticationService)(C.frida_static_authentication_service_new(tknC))
	} else if params.AuthenticationCallback != nil {
		authService = (*C.FridaAuthenticationService)(C.frida_go_authentication_service_new(unsafe.Pointer(&params.AuthenticationCallback)))
	}

	if params.Origin != "" {
		originC = C.CString(params.Origin)
		defer C.free(unsafe.Pointer(originC))
	}

	var cert *C.GTlsCertificate = nil
	if params.Certificate != "" {
		crt, err := gTLSCertificateFromFile(params.Certificate)
		if err != nil {
			return nil, err
		}
		cert = crt.cert
	}

	if params.AssetRoot != "" {
		assetPath = gFileFromPath(params.AssetRoot)
	}

	ret := C.frida_endpoint_parameters_new(
		addrC,
		C.guint16(params.Port),
		cert,
		originC,
		authService,
		assetPath,
	)

	return &EndpointParameters{ret}, nil
}

// Address returns the address of the endpoint parameters.
func (e *EndpointParameters) Address() string {
	return C.GoString(C.frida_endpoint_parameters_get_address(e.params))
}

// Port returns the port of the endpoint parameters.
func (e *EndpointParameters) Port() uint16 {
	return uint16(C.frida_endpoint_parameters_get_port(e.params))
}

// Certificate returns the certificate of the endpoint parameters.
func (e *EndpointParameters) Certificate() *Certificate {
	cert := C.frida_endpoint_parameters_get_certificate(e.params)
	return &Certificate{cert}
}

// Origin returns the origin of the endpoint parameters.
func (e *EndpointParameters) Origin() string {
	return C.GoString(C.frida_endpoint_parameters_get_origin(e.params))
}

// AssetRoot returns the asset root directory.
func (e *EndpointParameters) AssetRoot() string {
	assetRoot := C.frida_endpoint_parameters_get_asset_root(e.params)
	pathC := C.g_file_get_path(assetRoot)
	return C.GoString(pathC)
}

// SetAssetRoot sets asset root directory for the portal.
func (e *EndpointParameters) SetAssetRoot(assetPath string) {
	assetRoot := gFileFromPath(assetPath)
	C.frida_endpoint_parameters_set_asset_root(e.params, assetRoot)
}

// Clean will clean the resources held by the endpoint parameters.
func (e *EndpointParameters) Clean() {
	clean(unsafe.Pointer(e.params), unrefFrida)
}
