#pragma once

#include <glib-object.h>

/**
 * FooIndecision:
 * @FOO_MAYBE:     Something maybe
 * @FOO_POSSIBLY:  Something possible
 *
 * The indecision type.
 **/

typedef enum {
    FOO_MAYBE,
    FOO_POSSIBLY,
} FooIndecision;

/**
 * FooObjClass:
 *
 * The class
 */

/**
 * FooObj:
 *
 * The instance
 */

#define FOO_TYPE_OBJ foo_obj_get_type()
G_DECLARE_FINAL_TYPE(FooObj, foo_obj, FOO, OBJ, GObject)

int foo_do_something(FooObj *self);
