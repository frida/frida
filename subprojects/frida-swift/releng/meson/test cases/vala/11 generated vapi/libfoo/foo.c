#include "foo.h"

struct _FooFoo
{
  GObject parent_instance;
};

G_DEFINE_TYPE (FooFoo, foo_foo, G_TYPE_OBJECT)

static void
foo_foo_class_init (FooFooClass *klass)
{
}

static void
foo_foo_init (FooFoo *self)
{
}

/**
 * foo_foo_return_success:
 *
 * Returns 0
 */
int foo_foo_return_success(void)
{
  return 0;
}
