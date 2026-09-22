#include "frida-helper-backend.h"
#include "helpers/inject-context.h"

G_STATIC_ASSERT (sizeof (FridaHelperBootstrapContext) == sizeof (FridaBootstrapContext));
G_STATIC_ASSERT (sizeof (FridaHelperLoaderContext) == sizeof (FridaLoaderContext));
G_STATIC_ASSERT (sizeof (FridaHelperLibcApi) == sizeof (FridaLibcApi));
G_STATIC_ASSERT (sizeof (FridaHelperByeMessage) == sizeof (FridaByeMessage));
