#ifndef MESON_SUB_SAMPLE_H
#define MESON_SUB_SAMPLE_H

#if !defined (MESON_TEST)
#error "MESON_TEST not defined."
#endif

#include <glib-object.h>
#include <meson-sample.h>

G_BEGIN_DECLS

#define MESON_TYPE_SUB_SAMPLE (meson_sub_sample_get_type())

G_DECLARE_FINAL_TYPE (MesonSubSample, meson_sub_sample, MESON, SUB_SAMPLE, MesonSample)

MesonSubSample *meson_sub_sample_new           (const gchar *msg);

G_END_DECLS

#endif /* MESON_SUB_SAMPLE_H */
