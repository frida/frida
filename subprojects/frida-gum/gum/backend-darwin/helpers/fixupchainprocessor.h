/*
 * Copyright (C) 2020-2022 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_FIXUP_CHAIN_PROCESSOR_H__
#define __GUM_FIXUP_CHAIN_PROCESSOR_H__

#include <mach-o/loader.h>
#include <stddef.h>

# ifndef __GUM_DARWIN_MODULE_H__

typedef struct _GumChainedFixupsHeader GumChainedFixupsHeader;

typedef struct _GumChainedStartsInImage GumChainedStartsInImage;
typedef struct _GumChainedStartsInSegment GumChainedStartsInSegment;
typedef uint16_t GumChainedPtrFormat;

typedef struct _GumChainedPtr64Rebase GumChainedPtr64Rebase;
typedef struct _GumChainedPtr64Bind GumChainedPtr64Bind;
typedef struct _GumChainedPtrArm64eRebase GumChainedPtrArm64eRebase;
typedef struct _GumChainedPtrArm64eBind GumChainedPtrArm64eBind;
typedef struct _GumChainedPtrArm64eBind24 GumChainedPtrArm64eBind24;
typedef struct _GumChainedPtrArm64eAuthRebase GumChainedPtrArm64eAuthRebase;
typedef struct _GumChainedPtrArm64eAuthBind GumChainedPtrArm64eAuthBind;
typedef struct _GumChainedPtrArm64eAuthBind24 GumChainedPtrArm64eAuthBind24;

typedef uint32_t GumChainedImportFormat;
typedef uint32_t GumChainedSymbolFormat;

typedef struct _GumChainedImport GumChainedImport;
typedef struct _GumChainedImportAddend GumChainedImportAddend;
typedef struct _GumChainedImportAddend64 GumChainedImportAddend64;

struct _GumChainedFixupsHeader
{
  uint32_t fixups_version;
  uint32_t starts_offset;
  uint32_t imports_offset;
  uint32_t symbols_offset;
  uint32_t imports_count;
  GumChainedImportFormat imports_format;
  GumChainedSymbolFormat symbols_format;
};

struct _GumChainedStartsInImage
{
  uint32_t seg_count;
  uint32_t seg_info_offset[1];
};

struct _GumChainedStartsInSegment
{
  uint32_t size;
  uint16_t page_size;
  GumChainedPtrFormat pointer_format;
  uint64_t segment_offset;
  uint32_t max_valid_pointer;
  uint16_t page_count;
  uint16_t page_start[1];
};

enum _GumChainedPtrStart
{
  GUM_CHAINED_PTR_START_NONE  = 0xffff,
  GUM_CHAINED_PTR_START_MULTI = 0x8000,
  GUM_CHAINED_PTR_START_LAST  = 0x8000,
};

enum _GumChainedPtrFormat
{
  GUM_CHAINED_PTR_ARM64E              =  1,
  GUM_CHAINED_PTR_64                  =  2,
  GUM_CHAINED_PTR_32                  =  3,
  GUM_CHAINED_PTR_32_CACHE            =  4,
  GUM_CHAINED_PTR_32_FIRMWARE         =  5,
  GUM_CHAINED_PTR_64_OFFSET           =  6,
  GUM_CHAINED_PTR_ARM64E_OFFSET       =  7,
  GUM_CHAINED_PTR_ARM64E_KERNEL       =  7,
  GUM_CHAINED_PTR_64_KERNEL_CACHE     =  8,
  GUM_CHAINED_PTR_ARM64E_USERLAND     =  9,
  GUM_CHAINED_PTR_ARM64E_FIRMWARE     = 10,
  GUM_CHAINED_PTR_X86_64_KERNEL_CACHE = 11,
  GUM_CHAINED_PTR_ARM64E_USERLAND24   = 12,
};

struct _GumChainedPtr64Rebase
{
  uint64_t target   : 36,
           high8    :  8,
           reserved :  7,
           next     : 12,
           bind     :  1;
};

struct _GumChainedPtr64Bind
{
  uint64_t ordinal  : 24,
           addend   :  8,
           reserved : 19,
           next     : 12,
           bind     :  1;
};

struct _GumChainedPtrArm64eRebase
{
  uint64_t target : 43,
           high8  :  8,
           next   : 11,
           bind   :  1,
           auth   :  1;
};

struct _GumChainedPtrArm64eBind
{
  uint64_t ordinal : 16,
           zero    : 16,
           addend  : 19,
           next    : 11,
           bind    :  1,
           auth    :  1;
};

struct _GumChainedPtrArm64eBind24
{
  uint64_t ordinal : 24,
           zero    :  8,
           addend  : 19,
           next    : 11,
           bind    :  1,
           auth    :  1;
};

struct _GumChainedPtrArm64eAuthRebase
{
  uint64_t target    : 32,
           diversity : 16,
           addr_div  :  1,
           key       :  2,
           next      : 11,
           bind      :  1,
           auth      :  1;
};

struct _GumChainedPtrArm64eAuthBind
{
  uint64_t ordinal   : 16,
           zero      : 16,
           diversity : 16,
           addr_div  :  1,
           key       :  2,
           next      : 11,
           bind      :  1,
           auth      :  1;
};

struct _GumChainedPtrArm64eAuthBind24
{
  uint64_t ordinal   : 24,
           zero      :  8,
           diversity : 16,
           addr_div  :  1,
           key       :  2,
           next      : 11,
           bind      :  1,
           auth      :  1;
};

enum _GumChainedImportFormat
{
  GUM_CHAINED_IMPORT          = 1,
  GUM_CHAINED_IMPORT_ADDEND   = 2,
  GUM_CHAINED_IMPORT_ADDEND64 = 3,
};

enum _GumChainedSymbolFormat
{
  GUM_CHAINED_SYMBOL_UNCOMPRESSED,
  GUM_CHAINED_SYMBOL_ZLIB_COMPRESSED,
};

struct _GumChainedImport
{
  uint32_t lib_ordinal :  8,
           weak_import :  1,
           name_offset : 23;
};

struct _GumChainedImportAddend
{
  uint32_t lib_ordinal :  8,
           weak_import :  1,
           name_offset : 23;
  int32_t  addend;
};

struct _GumChainedImportAddend64
{
  uint64_t lib_ordinal : 16,
           weak_import :  1,
           reserved    : 15,
           name_offset : 32;
  uint64_t addend;
};

# endif

void gum_process_chained_fixups (const GumChainedFixupsHeader * fixups_header,
    struct mach_header_64 * mach_header, size_t preferred_base_address,
    void ** bound_pointers);

#endif
