#ifndef __DARWIN_SYSTEMCONFIGURATION_H__
#define __DARWIN_SYSTEMCONFIGURATION_H__

#ifndef HAVE_MACOS
# include <os/availability.h>
# undef API_UNAVAILABLE
# define API_UNAVAILABLE(...)
#endif

#include <SystemConfiguration/SystemConfiguration.h>

#endif
