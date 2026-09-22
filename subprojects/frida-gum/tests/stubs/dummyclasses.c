/*
 * Copyright (C) 2008-2018 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "dummyclasses.h"

struct _MyPony
{
  GObject parent;
};

struct _ZooZebra
{
  GObject parent;
};

G_DEFINE_TYPE (MyPony, my_pony, G_TYPE_OBJECT)
G_DEFINE_TYPE (ZooZebra, zoo_zebra, G_TYPE_OBJECT)

static void
my_pony_class_init (MyPonyClass * klass)
{
}

static void
my_pony_init (MyPony * self)
{
}

static void
zoo_zebra_class_init (ZooZebraClass * klass)
{
}

static void
zoo_zebra_init (ZooZebra * self)
{
}
