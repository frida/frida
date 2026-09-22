/*
 * Copyright (C) 2016-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_CODE_SEGMENT_H__
#define __GUM_CODE_SEGMENT_H__

#include <gum/gum.h>

G_BEGIN_DECLS

typedef struct _GumCodeSegment GumCodeSegment;

GUM_API gboolean gum_code_segment_is_supported (void);

GUM_API GumCodeSegment * gum_code_segment_new (gsize size,
    const GumAddressSpec * spec);
GUM_API void gum_code_segment_free (GumCodeSegment * segment);

GUM_API gpointer gum_code_segment_get_address (GumCodeSegment * self);
GUM_API gsize gum_code_segment_get_size (GumCodeSegment * self);
GUM_API gsize gum_code_segment_get_virtual_size (GumCodeSegment * self);

GUM_API void gum_code_segment_realize (GumCodeSegment * self);
GUM_API void gum_code_segment_map (GumCodeSegment * self, gsize source_offset,
    gsize source_size, gpointer target_address);

GUM_API gboolean gum_code_segment_mark (gpointer code, gsize size,
    GError ** error);

G_END_DECLS

#endif
