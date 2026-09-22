#ifndef __GUM_TVOS_H__
#define __GUM_TVOS_H__

#ifdef HAVE_TVOS
# include <Availability.h>
# undef __TVOS_PROHIBITED
# define __TVOS_PROHIBITED
# undef __API_UNAVAILABLE
# define __API_UNAVAILABLE(...)
# include <spawn.h>
# include <mach/mach.h>
# include <mach/task.h>
# undef __TVOS_PROHIBITED
# define __TVOS_PROHIBITED __OS_AVAILABILITY(tvos,unavailable)
#endif

#endif
