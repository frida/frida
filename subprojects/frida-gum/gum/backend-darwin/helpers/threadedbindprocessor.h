/*
 * Copyright (C) 2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_THREADED_BIND_PROCESSOR_H__
#define __GUM_THREADED_BIND_PROCESSOR_H__

#include <stdint.h>

void gum_process_threaded_items (uint64_t preferred_base_address,
    uint64_t slide, uint16_t num_symbols, const uint64_t * symbols,
    uint16_t num_regions, uint64_t * regions);

#endif
