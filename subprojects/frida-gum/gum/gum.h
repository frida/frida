/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_H__
#define __GUM_H__

#include <gum/gumdefs.h>

#include <gum/gumapiresolver.h>
#include <gum/gumbacktracer.h>
#include <gum/gumcontrolflowgraph.h>
#include <gum/gumcloak.h>
#include <gum/gumcodeallocator.h>
#include <gum/gumcodesegment.h>
#include <gum/gumdarwingrafter.h>
#include <gum/gumdarwinmodule.h>
#include <gum/gumelfmodule.h>
#include <gum/gumevent.h>
#include <gum/gumeventsink.h>
#include <gum/gumexceptor.h>
#include <gum/gumfunction.h>
#include <gum/guminterceptor.h>
#include <gum/guminvocationcontext.h>
#include <gum/guminvocationlistener.h>
#include <gum/gumkernel.h>
#include <gum/gumlibc.h>
#include <gum/gummemory.h>
#include <gum/gummemoryaccessmonitor.h>
#include <gum/gummemorymap.h>
#include <gum/gummetalarray.h>
#include <gum/gummetalhash.h>
#include <gum/gummodule.h>
#include <gum/gummoduleapiresolver.h>
#include <gum/gummodulemap.h>
#include <gum/gummoduleregistry.h>
#include <gum/gumprintf.h>
#include <gum/gumprocess.h>
#include <gum/gumreturnaddress.h>
#include <gum/gumspinlock.h>
#include <gum/gumstalker.h>
#include <gum/gumsymbolutil.h>
#include <gum/gumsysinternals.h>
#include <gum/gumthreadregistry.h>
#include <gum/gumtls.h>
#include <gum/gumunwindbroker.h>

G_BEGIN_DECLS

GUM_API void gum_init (void);
GUM_API void gum_shutdown (void);
GUM_API void gum_deinit (void);

GUM_API void gum_init_embedded (void);
GUM_API void gum_deinit_embedded (void);

GUM_API void gum_prepare_to_fork (void);
GUM_API void gum_recover_from_fork_in_parent (void);
GUM_API void gum_recover_from_fork_in_child (void);

G_END_DECLS

#endif
