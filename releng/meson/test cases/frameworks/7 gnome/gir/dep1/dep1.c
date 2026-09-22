#include "dep1.h"

struct _MesonDep1
{
  GObject parent_instance;
};

G_DEFINE_TYPE (MesonDep1, meson_dep1, G_TYPE_OBJECT)

/**
 * meson_dep1_new:
 *
 * Allocates a new #MesonDep1.
 *
 * Returns: (transfer full): a #MesonDep1.
 */
MesonDep1 *
meson_dep1_new (void)
{
  return g_object_new (MESON_TYPE_DEP1, NULL);
}

static void
meson_dep1_finalize (GObject *object)
{
  G_OBJECT_CLASS (meson_dep1_parent_class)->finalize (object);
}

static void
meson_dep1_class_init (MesonDep1Class *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = meson_dep1_finalize;
}

static void
meson_dep1_init (MesonDep1 *self)
{
}

/**
 * meson_dep1_just_return_it:
 * @dep: a #MesonDep2.
 *
 * Returns the #MesonDep2 that is passed in
 *
 * Returns: (transfer none): a #MesonDep2
 */
MesonDep2*
meson_dep1_just_return_it (MesonDep1 *self, MesonDep2 *dep)
{
  g_return_val_if_fail (MESON_IS_DEP1 (self), NULL);

  return dep;
}
