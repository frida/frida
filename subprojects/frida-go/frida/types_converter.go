package frida

/*
#include <frida-core.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

extern void getSVArray(gchar*,GVariant*,char*);
extern void getASVArray(gchar*,GVariant*,char*);

static char * get_gvalue_gtype(GValue * val) {
	return (char*)(G_VALUE_TYPE_NAME(val));
}

static char * get_ip_str(const struct sockaddr *sa, size_t maxlen)
{
	char * dest = malloc(sizeof(char) * maxlen);
    switch(sa->sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
                    dest, maxlen);
            break;

        case AF_INET6:
            inet_ntop(AF_INET6, &(((struct sockaddr_in6 *)sa)->sin6_addr),
                    dest, maxlen);
            break;

        default:
            strncpy(dest, "Unknown AF", maxlen);
            return NULL;
    }

    return dest;
}

static int get_in_port(struct sockaddr *sa)
{
	in_port_t port;
    if (sa->sa_family == AF_INET)
        port = (((struct sockaddr_in*)sa)->sin_port);

    port = (((struct sockaddr_in6*)sa)->sin6_port);

	return (int)port;
}

static struct sockaddr * new_addr() {
	struct sockaddr * addr = malloc(sizeof(struct sockaddr));
	return addr;
}


static char ** new_char_array(int size) {
	return malloc(sizeof(char*) * size);
}
static void add_to_arr(char **arr, char *s, int n) {
	arr[n] = s;
}
static char * get_char_elem(char **arr, int n) {
	return arr[n];
}
static guint8 * new_guint8_array(int size) {
	return malloc(sizeof(guint8) * size);
}
static void att_to_guint8_array(guint8 * arr, guint8 elem, int n) {
	arr[n] = elem;
}

static void iter_array(GVariant *var, char *aData) {
	GVariantIter iter;
	GVariant *value;
	gchar *key;

	g_variant_iter_init (&iter, var);
	while (g_variant_iter_loop (&iter, "{sv}", &key, &value))
	{
			getSVArray(key, value, aData);
	}
}

static void iter_double_arr(GVariant *var, char *mData) {
	GVariantIter iter1;
	GVariantIter *iter2;

	g_variant_iter_init(&iter1, var);
	while (g_variant_iter_loop (&iter1, "a{sv}", &iter2)) {
		GVariant *val;
		gchar *key;

		while (g_variant_iter_loop(iter2, "{sv}", &key, &val)) {
			gchar * tp;
			tp = (char*)g_variant_get_type_string(val);
			getASVArray(key, val, mData);
		}
	}
}

static size_t getElemsSize() {
	return sizeof(guint8);
}
*/
import "C"
import (
	"unsafe"
)

type gTypeName string

const (
	gchararray               gTypeName = "gchararray"
	gBytes                   gTypeName = "GBytes"
	fridaCrash               gTypeName = "FridaCrash"
	fridaSessionDetachReason gTypeName = "FridaSessionDetachReason"
	fridaChild               gTypeName = "FridaChild"
	fridaDevice              gTypeName = "FridaDevice"
	fridaApplication         gTypeName = "FridaApplication"
	guint                    gTypeName = "guint"
	gint                     gTypeName = "gint"
	gFileMonitorEvent        gTypeName = "GFileMonitorEvent"
	gSocketAddress           gTypeName = "GSocketAddress"
	gVariant                 gTypeName = "GVariant"
)

type marshallerFunc func(val *C.GValue) any

var gTypeString = map[gTypeName]marshallerFunc{
	gchararray:               getString,
	gBytes:                   getGBytesV,
	fridaCrash:               getFridaCrash,
	fridaSessionDetachReason: getFridaSessionDetachReason,
	fridaChild:               getFridaChild,
	fridaDevice:              getFridaDevice,
	fridaApplication:         getFridaApplication,
	guint:                    getInt,
	gint:                     getInt,
	gFileMonitorEvent:        getFm,
	gSocketAddress:           getGSocketAddress,
	gVariant:                 getGVariant,
}

func getString(val *C.GValue) any {
	cc := C.g_value_get_string(val)
	return C.GoString(cc)
}

func getGBytesV(val *C.GValue) any {
	if val != nil {
		obj := (*C.GBytes)(C.g_value_get_object(val))
		return getGBytes(obj)
	}
	return []byte{}
}

func getGBytes(obj *C.GBytes) []byte {
	if obj != nil {
		bytesC := (*C.guint8)(C.g_bytes_get_data(obj, nil))
		sz := C.g_bytes_get_size(obj)

		return C.GoBytes(unsafe.Pointer(bytesC), C.int(sz))
	}
	return []byte{}
}

func getFridaCrash(val *C.GValue) any {
	crash := (*C.FridaCrash)(C.g_value_get_object(val))

	return &Crash{
		crash: crash,
	}
}

func getFridaSessionDetachReason(val *C.GValue) any {
	reason := C.g_value_get_int(val)
	return SessionDetachReason(int(reason))
}

func getFridaChild(val *C.GValue) any {
	child := (*C.FridaChild)(C.g_value_get_object(val))

	return &Child{
		child: child,
	}
}

func getFridaDevice(val *C.GValue) any {
	dev := (*C.FridaDevice)(C.g_value_get_object(val))

	return &Device{
		device: dev,
	}
}

func getInt(val *C.GValue) any {
	v := C.g_value_get_int(val)
	return int(v)
}

func getFm(val *C.GValue) any {
	v := C.int(C.g_value_get_int(val))

	vals := map[int]string{
		0: "changed",
		1: "changes-done-hint",
		2: "deleted",
		3: "created",
		4: "attribute-changed",
		5: "pre-mount",
		6: "unmounted",
		7: "moved",
	}

	return vals[int(v)]
}

func getGSocketAddress(val *C.GValue) any {
	obj := (*C.GSocketAddress)(C.g_value_get_object(val))
	sz := C.g_socket_address_get_native_size(obj)
	dest := C.new_addr()
	var err *C.GError
	C.g_socket_address_to_native(obj, (C.gpointer)(dest), C.gsize(sz), &err)
	if err != nil {
		panic(err)
	}

	s := C.get_ip_str(dest, C.size_t(sz))
	port := C.get_in_port(dest)

	return &Address{
		Addr: C.GoString(s),
		Port: uint16(port),
	}
}

func getFridaApplication(val *C.GValue) any {
	app := (*C.FridaApplication)(C.g_value_get_object(val))

	return &Application{
		application: app,
	}
}

func getGVariant(val *C.GValue) any {
	v := C.g_value_get_variant(val)
	return gVariantToGo(v)
}

func getGoValueFromGValue(val *C.GValue) any {
	gt := C.get_gvalue_gtype(val)

	f, ok := gTypeString[gTypeName(C.GoString(gt))]
	if !ok {
		return struct{}{}
	}

	return f(val)
}

func getCharArrayElement(arr **C.char, n int) *C.char {
	elem := C.get_char_elem(arr, C.int(n))
	return elem
}

func cArrayToStringSlice(arr **C.char, length C.int) []string {
	s := make([]string, int(length))

	for i := 0; i < int(length); i++ {
		elem := C.get_char_elem(arr, C.int(i))
		s[i] = C.GoString(elem)
	}

	return s
}

func stringSliceToCharArr(ss []string) (**C.char, C.int) {
	arr := C.new_char_array(C.int(len(ss)))

	for i, s := range ss {
		C.add_to_arr(arr, C.CString(s), C.int(i))
	}

	return arr, C.int(len(ss))
}

func goBytesToGBytes(bts []byte) *C.GBytes {
	arr := C.new_guint8_array(C.int(len(bts)))

	for i, bt := range bts {
		C.att_to_guint8_array(arr, C.guint8(bt), C.int(i))
	}

	gBytes := C.g_bytes_new((C.gconstpointer)(arr), C.gsize(len(bts)))
	return gBytes
}

func gHashTableToMap(ht *C.GHashTable) map[string]any {
	iter := C.GHashTableIter{}
	var key C.gpointer
	var val C.gpointer

	C.g_hash_table_iter_init(&iter, ht)

	hSize := int(C.g_hash_table_size(ht))
	var data map[string]any
	//data := make(map[string]any)

	if hSize >= 1 {
		data = make(map[string]any, hSize)
		nx := 1
		for nx == 1 {
			nx = int(C.g_hash_table_iter_next(&iter, &key, &val))

			keyGo := C.GoString((*C.char)(unsafe.Pointer(key)))
			valGo := gPointerToGo(val)

			data[keyGo] = valGo
		}
	}

	return data
}

type arrData struct {
	mapper map[string]any
}

type multiData struct {
	mapper map[string]any
}

//export getSVArray
func getSVArray(key *C.gchar, variant *C.GVariant, aData *C.char) {
	k := C.GoString((*C.char)(key))
	v := stringFromVariant(variant)
	arrayData := (*arrData)(unsafe.Pointer(aData))
	arrayData.mapper[k] = v
}

//export getASVArray
func getASVArray(key *C.gchar, variant *C.GVariant, mData *C.char) {
	keyGo := C.GoString(key)
	vType := getVariantStringFormat(variant)
	multData := (*multiData)(unsafe.Pointer(mData))
	if vType == "s" {
		multData.mapper[keyGo] = stringFromVariant(variant)
	} else {
		multData.mapper[keyGo] = bytesFromVariant(variant)
	}
}

func getVariantStringFormat(variant *C.GVariant) string {
	variantString := ""
	if variant != nil {
		variantType := C.g_variant_get_type_string(variant)
		variantString = C.GoString((*C.char)(variantType))
	}
	return variantString
}

func bytesFromVariant(variant *C.GVariant) []byte {
	var res *C.char
	sz := C.g_variant_get_size(variant)
	res = (*C.char)((C.g_variant_get_data(variant)))
	bts := C.GoBytes(unsafe.Pointer(res), C.int(sz))
	return bts
}

// stringFromVariant extracts bool ("b") from GVariant
func stringFromVariant(variant *C.GVariant) string {
	var sz C.gsize
	return C.GoString(C.g_variant_get_string(variant, &sz))
}

// stringFromVariant extracts string ("s") from GVariant
func boolFromVariant(variant *C.GVariant) bool {
	val := C.g_variant_get_boolean(variant)
	return int(val) != 0
}

func int64FromVariant(variant *C.GVariant) int64 {
	val := C.g_variant_get_int64(variant)
	return int64(val)
}

func gPointerToGo(ptr C.gpointer) any {
	variant := (*C.GVariant)(ptr)
	variantType := getVariantStringFormat(variant)

	switch variantType {
	case "s":
		return stringFromVariant(variant)
	case "b":
		return boolFromVariant(variant)
	case "x":
		return int64FromVariant(variant)
	case "a{sv}":
		mp := make(map[string]any)
		aData := arrData{
			mapper: mp,
		}
		C.iter_array(variant, (*C.char)(unsafe.Pointer(&aData)))
		return aData.mapper
	case "aa{sv}":
		mp := make(map[string]any)
		mData := multiData{
			mapper: mp,
		}
		C.iter_double_arr(variant, (*C.char)(unsafe.Pointer(&mData)))
		return mData.mapper
	default:
		return variantType
	}
}

func gVariantToGo(variant *C.GVariant) any {
	return gPointerToGo((C.gpointer)(variant))
}
