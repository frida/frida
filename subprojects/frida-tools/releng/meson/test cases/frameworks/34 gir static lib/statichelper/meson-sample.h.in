#ifndef MESON_SAMPLE_H
#define MESON_SAMPLE_H

#include <@HEADER@>

G_BEGIN_DECLS

#define MESON_TYPE_SAMPLE (meson_sample_get_type())

G_DECLARE_DERIVABLE_TYPE (MesonSample, meson_sample, MESON, SAMPLE, GObject)

struct _MesonSampleClass {
    GObjectClass parent_class;
};


MesonSample *meson_sample_new           (const gchar *msg);
void         meson_sample_print_message (MesonSample *self);

G_END_DECLS

#endif /* MESON_SAMPLE_H */
