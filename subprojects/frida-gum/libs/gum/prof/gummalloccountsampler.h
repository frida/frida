/*
 * Copyright (C) 2008-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_MALLOC_COUNT_SAMPLER_H__
#define __GUM_MALLOC_COUNT_SAMPLER_H__

#include "gumsampler.h"

#include <gum/gumheapapi.h>

G_BEGIN_DECLS

GUM_API GumSampler * gum_malloc_count_sampler_new (void);
GUM_API GumSampler * gum_malloc_count_sampler_new_with_heap_apis (
    const GumHeapApiList * heap_apis);

G_END_DECLS

#endif
