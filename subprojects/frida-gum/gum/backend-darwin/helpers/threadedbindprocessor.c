/*
 * Copyright (C) 2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "threadedbindprocessor.h"

#include <ptrauth.h>
#include <stdbool.h>

#define GUM_INT2_MASK  0x00000003U
#define GUM_INT11_MASK 0x000007ffU
#define GUM_INT16_MASK 0x0000ffffU
#define GUM_INT32_MASK 0xffffffffU

typedef uint8_t GumDarwinThreadedItemType;

enum _GumDarwinThreadedItemType
{
  GUM_DARWIN_THREADED_REBASE,
  GUM_DARWIN_THREADED_BIND
};

static void * gum_sign_pointer (void * ptr, uint8_t key, uintptr_t diversity,
    bool use_address_diversity, void * address_of_ptr);

void
gum_process_threaded_items (uint64_t preferred_base_address,
                            uint64_t slide,
                            uint16_t num_symbols,
                            const uint64_t * symbols,
                            uint16_t num_regions,
                            uint64_t * regions)
{
  uint16_t i;

  for (i = 0; i != num_regions; i++)
  {
    uint64_t * slot = (uint64_t *) regions[i];
    uint16_t delta;

    do
    {
      uint64_t value;
      bool is_authenticated;
      GumDarwinThreadedItemType type;
      uint8_t key;
      bool has_address_diversity;
      uint16_t diversity;
      uint64_t bound_value;

      value = *slot;

      is_authenticated      = (value >> 63) & 1;
      type                  = (value >> 62) & 1;
      delta                 = (value >> 51) & GUM_INT11_MASK;
      key                   = (value >> 49) & GUM_INT2_MASK;
      has_address_diversity = (value >> 48) & 1;
      diversity             = (value >> 32) & GUM_INT16_MASK;

      if (type == GUM_DARWIN_THREADED_BIND)
      {
        uint16_t bind_ordinal;

        bind_ordinal = value & GUM_INT16_MASK;

        bound_value = symbols[bind_ordinal];
      }
      else if (type == GUM_DARWIN_THREADED_REBASE)
      {
        uint64_t rebase_address;

        if (is_authenticated)
        {
          rebase_address = value & GUM_INT32_MASK;
        }
        else
        {
          uint64_t top_8_bits, bottom_43_bits, sign_bits;
          bool sign_bit_set;

          top_8_bits = (value << 13) & 0xff00000000000000UL;
          bottom_43_bits = value     & 0x000007ffffffffffUL;

          sign_bit_set = (value >> 42) & 1;
          if (sign_bit_set)
            sign_bits = 0x00fff80000000000UL;
          else
            sign_bits = 0;

          rebase_address = top_8_bits | sign_bits | bottom_43_bits;
        }

        bound_value = rebase_address;

        if (is_authenticated)
          bound_value += preferred_base_address;

        bound_value += slide;
      }

      if (is_authenticated)
      {
        *slot = (uint64_t) gum_sign_pointer ((void *) bound_value, key,
            diversity, has_address_diversity, slot);
      }
      else
      {
        *slot = bound_value;
      }

      slot += delta;
    }
    while (delta != 0);
  }
}

static void *
gum_sign_pointer (void * ptr,
                  uint8_t key,
                  uintptr_t diversity,
                  bool use_address_diversity,
                  void * address_of_ptr)
{
  void * p = ptr;
  uintptr_t d = diversity;

  if (use_address_diversity)
    d = ptrauth_blend_discriminator (address_of_ptr, d);

  switch (key)
  {
    case ptrauth_key_asia:
      p = ptrauth_sign_unauthenticated (p, ptrauth_key_asia, d);
      break;
    case ptrauth_key_asib:
      p = ptrauth_sign_unauthenticated (p, ptrauth_key_asib, d);
      break;
    case ptrauth_key_asda:
      p = ptrauth_sign_unauthenticated (p, ptrauth_key_asda, d);
      break;
    case ptrauth_key_asdb:
      p = ptrauth_sign_unauthenticated (p, ptrauth_key_asdb, d);
      break;
  }

  return p;
}
