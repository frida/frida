#ifndef MESON_DEP3_H
#define MESON_DEP3_H

#if !defined (MESON_TEST_1)
#error "MESON_TEST_1 not defined."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define MESON_TYPE_DEP3 (meson_dep3_get_type())

G_DECLARE_FINAL_TYPE (MesonDep3, meson_dep3, MESON, DEP3, GObject)

MesonDep3   *meson_dep3_new            (const gchar *msg);
const gchar *meson_dep3_return_message (MesonDep3 *self);

G_END_DECLS

#endif /* MESON_DEP3_H */
