/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __DUMMY_CLASSES_H__
#define __DUMMY_CLASSES_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define MY_TYPE_PONY (my_pony_get_type ())
G_DECLARE_FINAL_TYPE (MyPony, my_pony, MY, PONY, GObject)

#define ZOO_TYPE_ZEBRA (zoo_zebra_get_type ())
G_DECLARE_FINAL_TYPE (ZooZebra, zoo_zebra, ZOO, ZEBRA, GObject)

G_END_DECLS

#endif
