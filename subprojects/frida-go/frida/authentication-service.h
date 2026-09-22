#ifndef __AUTHENTICATION_SERVICE_H__
#define __AUTHENTICATION_SERVICE_H__

#include <frida-core.h>

#define FRIDA_TYPE_GO_AUTHENTICATION_SERVICE (frida_go_authentication_service_get_type ())
G_DECLARE_FINAL_TYPE (GoAuthenticationService, frida_go_authentication_service, FRIDA, GO_AUTHENTICATION_SERVICE, GObject)

GoAuthenticationService * frida_go_authentication_service_new (void * callback);

#endif 