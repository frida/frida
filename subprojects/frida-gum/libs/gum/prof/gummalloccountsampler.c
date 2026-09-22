/*
 * Copyright (C) 2008-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gummalloccountsampler.h"

#include "gumcallcountsampler.h"
#include "guminterceptor.h"

#include <stdlib.h>

GumSampler *
gum_malloc_count_sampler_new (void)
{
  return gum_call_count_sampler_new (
      GUM_FUNCPTR_TO_POINTER (malloc),
      GUM_FUNCPTR_TO_POINTER (calloc),
      GUM_FUNCPTR_TO_POINTER (realloc),
      NULL);
}

GumSampler *
gum_malloc_count_sampler_new_with_heap_apis (const GumHeapApiList * heap_apis)
{
  GumCallCountSampler * sampler;
  GumInterceptor * interceptor;
  guint i;

  interceptor = gum_interceptor_obtain ();
  gum_interceptor_ignore_current_thread (interceptor);
  gum_interceptor_begin_transaction (interceptor);

  sampler = GUM_CALL_COUNT_SAMPLER (gum_call_count_sampler_new (NULL));

  for (i = 0; i != heap_apis->len; i++)
  {
    const GumHeapApi * api = gum_heap_api_list_get_nth (heap_apis, i);

    gum_call_count_sampler_add_function (sampler,
        GUM_FUNCPTR_TO_POINTER (api->malloc));
    gum_call_count_sampler_add_function (sampler,
        GUM_FUNCPTR_TO_POINTER (api->calloc));
    gum_call_count_sampler_add_function (sampler,
        GUM_FUNCPTR_TO_POINTER (api->realloc));
  }

  gum_interceptor_end_transaction (interceptor);
  gum_interceptor_unignore_current_thread (interceptor);
  g_object_unref (interceptor);

  return GUM_SAMPLER (sampler);
}
