#ifndef MESON_SAMPLE_H
#define MESON_SAMPLE_H

#if !defined (MESON_TEST_1)
#error "MESON_TEST_1 not defined."
#endif

#if !defined (MESON_TEST_2)
#error "MESON_TEST_2 not defined."
#endif

#include <glib-object.h>
#include "dep1/dep1.h"

G_BEGIN_DECLS

#define MESON_TYPE_SAMPLE (meson_sample_get_type())

G_DECLARE_FINAL_TYPE (MesonSample, meson_sample, MESON, SAMPLE, GObject)

MesonSample *meson_sample_new           (void);
void         meson_sample_print_message (MesonSample *self,
                                         MesonDep1 *dep1,
                                         MesonDep2 *dep2);

G_END_DECLS

#endif /* MESON_SAMPLE_H */
