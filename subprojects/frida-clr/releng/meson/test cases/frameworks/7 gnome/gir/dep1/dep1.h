#ifndef MESON_DEP1_H
#define MESON_DEP1_H

#if !defined (MESON_TEST_1)
#error "MESON_TEST_1 not defined."
#endif

#include <glib-object.h>
#include "dep2/dep2.h"

G_BEGIN_DECLS

#define MESON_TYPE_DEP1 (meson_dep1_get_type())

G_DECLARE_FINAL_TYPE (MesonDep1, meson_dep1, MESON, DEP1, GObject)

MesonDep1   *meson_dep1_new            (void);
MesonDep2   *meson_dep1_just_return_it (MesonDep1 *self,
                                        MesonDep2 *dep);

G_END_DECLS

#endif /* MESON_DEP1_H */
