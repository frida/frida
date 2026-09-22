#include <glib-object.h>

#pragma once

#define FOO_TYPE_FOO (foo_foo_get_type())

G_DECLARE_FINAL_TYPE (FooFoo, foo_foo, Foo, FOO, GObject)

int foo_foo_return_success(void);
