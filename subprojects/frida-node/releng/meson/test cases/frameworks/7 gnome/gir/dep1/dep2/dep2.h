#ifndef MESON_DEP2_H
#define MESON_DEP2_H

#if !defined (MESON_TEST_1)
#error "MESON_TEST_1 not defined."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define MESON_TYPE_DEP2 (meson_dep2_get_type())

G_DECLARE_FINAL_TYPE (MesonDep2, meson_dep2, MESON, DEP2, GObject)

MesonDep2   *meson_dep2_new            (const gchar *msg);
const gchar *meson_dep2_return_message (MesonDep2 *self);

G_END_DECLS

#endif /* MESON_DEP2_H */
