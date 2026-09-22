#include "bar.h"
#include "foo.h"

struct _BarBar
{
  GObject parent_instance;
};

G_DEFINE_TYPE (BarBar, bar_bar, G_TYPE_OBJECT)

static void
bar_bar_class_init (BarBarClass *klass)
{
}

static void
bar_bar_init (BarBar *self)
{
}

/**
 * bar_bar_return_success:
 *
 * Returns 0
 */
int bar_bar_return_success(void)
{
  return foo_foo_return_success();
}
