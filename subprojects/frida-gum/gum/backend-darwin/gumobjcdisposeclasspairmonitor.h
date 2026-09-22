/*
 * Copyright (C) 2021 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_OBJC_DISPOSE_CLASS_PAIR_MONITOR_H__
#define __GUM_OBJC_DISPOSE_CLASS_PAIR_MONITOR_H__

#include <gum/guminterceptor.h>

G_BEGIN_DECLS

#define GUM_TYPE_OBJC_DISPOSE_CLASS_PAIR_MONITOR \
    (gum_objc_dispose_class_pair_monitor_get_type ())
G_DECLARE_FINAL_TYPE (GumObjcDisposeClassPairMonitor,
                      gum_objc_dispose_class_pair_monitor,
                      GUM, OBJC_DISPOSE_CLASS_PAIR_MONITOR,
                      GObject)

struct _GumObjcDisposeClassPairMonitor
{
  GObject parent;
  GRecMutex mutex;
  GumInterceptor * interceptor;
};

G_GNUC_INTERNAL GumObjcDisposeClassPairMonitor *
    gum_objc_dispose_class_pair_monitor_obtain (void);

G_END_DECLS

#endif
