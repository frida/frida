#include <foo.h>


struct _FooObj {
  GObject parent;
  int dummy;
};

G_DEFINE_TYPE(FooObj, foo_obj, G_TYPE_OBJECT)

static void foo_obj_init (FooObj *self)
{
}

static void foo_obj_class_init (FooObjClass *klass)
{
}

/**
 * foo_do_something:
 * @self: self
 *
 * Useless function.
 *
 * Returns: 0.
 */
int foo_do_something(FooObj *self)
{
  return 0;
}
