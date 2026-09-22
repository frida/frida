package frida

//#include <frida-core.h>
import "C"
import (
	"time"
	"unsafe"
)

const (
	cTimeFormat = "%Y-%m-%d %H:%M:%S"
	timeFormat  = "2006-01-02 15:04:05"
)

// Certificate represents the GTlsCertificate.
type Certificate struct {
	cert *C.GTlsCertificate
}

// IssuerName returns the issuer name for the certificate.
func (c *Certificate) IssuerName() string {
	iss := C.g_tls_certificate_get_issuer_name(c.cert)
	defer C.free(unsafe.Pointer(iss))
	return C.GoString(iss)
}

// SubjectName returns the subject name for the certificate.
func (c *Certificate) SubjectName() string {
	sub := C.g_tls_certificate_get_subject_name(c.cert)
	defer C.free(unsafe.Pointer(sub))
	return C.GoString(sub)
}

// NotValidBefore returns the time before which certificate is not valid.
func (c *Certificate) NotValidBefore() (time.Time, error) {
	vld := C.g_tls_certificate_get_not_valid_before(c.cert)
	frmt := C.CString(cTimeFormat)
	defer C.free(unsafe.Pointer(frmt))

	cc := C.g_date_time_format(vld, frmt)
	defer C.free(unsafe.Pointer(cc))

	return time.Parse(timeFormat, C.GoString(cc))
}

// NotValidAfter returns the time after which certificate is not valid.
func (c *Certificate) NotValidAfter() (time.Time, error) {
	vld := C.g_tls_certificate_get_not_valid_after(c.cert)
	frmt := C.CString(cTimeFormat)
	defer C.free(unsafe.Pointer(frmt))

	cc := C.g_date_time_format(vld, frmt)
	defer C.free(unsafe.Pointer(cc))

	return time.Parse(timeFormat, C.GoString(cc))
}
