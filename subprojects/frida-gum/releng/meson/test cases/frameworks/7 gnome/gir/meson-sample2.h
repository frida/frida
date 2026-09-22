#ifndef MESON_SAMPLE2_H
#define MESON_SAMPLE2_H

#if !defined (MESON_TEST_1)
#error "MESON_TEST_1 not defined."
#endif

#include <glib-object.h>

G_BEGIN_DECLS

#define MESON_TYPE_SAMPLE2 (meson_sample2_get_type())

G_DECLARE_FINAL_TYPE (MesonSample2, meson_sample2, MESON, SAMPLE2, GObject)

MesonSample2 *meson_sample2_new           (void);
void          meson_sample2_print_message (MesonSample2 *self);

G_END_DECLS

#endif /* MESON_SAMPLE2_H */
