/*
 * Copyright (C) 2020-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "fixupchainprocessor.h"

#include <ptrauth.h>
#include <stdbool.h>

static void gum_process_chained_fixups_in_segment_generic64 (void * cursor,
    GumChainedPtrFormat format, uint64_t actual_base_address,
    uint64_t preferred_base_address, void ** bound_pointers);
#ifdef __arm64e__
static void gum_process_chained_fixups_in_segment_arm64e (void * cursor,
    GumChainedPtrFormat format, uint64_t actual_base_address,
    uint64_t preferred_base_address, void ** bound_pointers);
static void * gum_sign_pointer (void * ptr, uint8_t key, uintptr_t diversity,
    bool use_address_diversity, void * address_of_ptr);
static int64_t gum_sign_extend_int19 (uint64_t i19);
#endif

void
gum_process_chained_fixups (const GumChainedFixupsHeader * fixups_header,
                            struct mach_header_64 * mach_header,
                            size_t preferred_base_address,
                            void ** bound_pointers)
{
  const GumChainedStartsInImage * image_starts;
  uint32_t seg_index;

  image_starts = (const GumChainedStartsInImage *)
      ((const void *) fixups_header + fixups_header->starts_offset);

  for (seg_index = 0; seg_index != image_starts->seg_count; seg_index++)
  {
    const uint32_t seg_offset = image_starts->seg_info_offset[seg_index];
    const GumChainedStartsInSegment * seg_starts;
    GumChainedPtrFormat format;
    uint16_t page_index;

    if (seg_offset == 0)
      continue;

    seg_starts = (const GumChainedStartsInSegment *)
        ((const void *) image_starts + seg_offset);
    format = seg_starts->pointer_format;

    for (page_index = 0; page_index != seg_starts->page_count; page_index++)
    {
      uint16_t start;
      void * cursor;

      start = seg_starts->page_start[page_index];
      if (start == GUM_CHAINED_PTR_START_NONE)
        continue;
      /* Ignoring MULTI for now as it only applies to 32-bit formats. */

      cursor = (void *) mach_header + seg_starts->segment_offset +
          (page_index * seg_starts->page_size) +
          start;

      if (format == GUM_CHAINED_PTR_64 || format == GUM_CHAINED_PTR_64_OFFSET)
      {
        gum_process_chained_fixups_in_segment_generic64 (cursor, format,
            (uintptr_t) mach_header, preferred_base_address, bound_pointers);
      }
      else
      {
#ifdef __arm64e__
        gum_process_chained_fixups_in_segment_arm64e (cursor, format,
            (uintptr_t) mach_header, preferred_base_address, bound_pointers);
#else
        __builtin_unreachable ();
#endif
      }
    }
  }
}

static void
gum_process_chained_fixups_in_segment_generic64 (
    void * cursor,
    GumChainedPtrFormat format,
    uint64_t actual_base_address,
    uint64_t preferred_base_address,
    void ** bound_pointers)
{
  const int64_t slide = actual_base_address - preferred_base_address;
  const size_t stride = 4;

  while (TRUE)
  {
    uint64_t * slot = cursor;
    size_t delta;

    if ((*slot >> 63) == 0)
    {
      GumChainedPtr64Rebase * item = cursor;
      uint64_t top_8_bits, bottom_36_bits, unpacked_target;

      delta = item->next;

      top_8_bits = (uint64_t) item->high8 << (64 - 8);
      bottom_36_bits = item->target;
      unpacked_target = top_8_bits | bottom_36_bits;

      if (format == GUM_CHAINED_PTR_64_OFFSET)
        *slot = actual_base_address + unpacked_target;
      else
        *slot = unpacked_target + slide;
    }
    else
    {
      GumChainedPtr64Bind * item = cursor;

      delta = item->next;

      *slot = (uint64_t) (bound_pointers[item->ordinal] + item->addend);
    }

    if (delta == 0)
      break;

    cursor += delta * stride;
  }
}

#ifdef __arm64e__

static void
gum_process_chained_fixups_in_segment_arm64e (void * cursor,
                                              GumChainedPtrFormat format,
                                              uint64_t actual_base_address,
                                              uint64_t preferred_base_address,
                                              void ** bound_pointers)
{
  const int64_t slide = actual_base_address - preferred_base_address;
  const size_t stride = 8;

  while (TRUE)
  {
    uint64_t * slot = cursor;
    size_t delta;

    switch (*slot >> 62)
    {
      case 0b00:
      {
        GumChainedPtrArm64eRebase * item = cursor;
        uint64_t top_8_bits, bottom_43_bits, unpacked_target;

        delta = item->next;

        top_8_bits = (uint64_t) item->high8 << (64 - 8);
        bottom_43_bits = item->target;

        unpacked_target = top_8_bits | bottom_43_bits;

        if (format == GUM_CHAINED_PTR_ARM64E)
          *slot = unpacked_target + slide;
        else
          *slot = actual_base_address + unpacked_target;

        break;
      }
      case 0b01:
      {
        GumChainedPtrArm64eBind * item = cursor;
        GumChainedPtrArm64eBind24 * item24 = cursor;
        uint32_t ordinal;

        delta = item->next;

        ordinal = (format == GUM_CHAINED_PTR_ARM64E_USERLAND24)
            ? item24->ordinal
            : item->ordinal;

        *slot = (uint64_t) (bound_pointers[ordinal] +
            gum_sign_extend_int19 (item->addend));

        break;
      }
      case 0b10:
      {
        GumChainedPtrArm64eAuthRebase * item = cursor;

        delta = item->next;

        *slot = (uint64_t) gum_sign_pointer (
            (void *) (preferred_base_address + item->target + slide),
            item->key, item->diversity, item->addr_div, slot);

        break;
      }
      case 0b11:
      {
        GumChainedPtrArm64eAuthBind * item = cursor;
        GumChainedPtrArm64eAuthBind24 * item24 = cursor;
        uint32_t ordinal;

        delta = item->next;

        ordinal = (format == GUM_CHAINED_PTR_ARM64E_USERLAND24)
            ? item24->ordinal
            : item->ordinal;

        *slot = (uint64_t) gum_sign_pointer (bound_pointers[ordinal],
            item->key, item->diversity, item->addr_div, slot);

        break;
      }
    }

    if (delta == 0)
      break;

    cursor += delta * stride;
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

static int64_t
gum_sign_extend_int19 (uint64_t i19)
{
  int64_t result;
  bool sign_bit_set;

  result = i19;

  sign_bit_set = i19 >> (19 - 1);
  if (sign_bit_set)
    result |= 0xfffffffffff80000ULL;

  return result;
}

#endif
