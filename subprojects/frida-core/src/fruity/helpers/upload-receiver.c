#include "upload-api.h"

#include <ptrauth.h>
#include <stdbool.h>
#include <stdint.h>
#include <mach-o/loader.h>

#define FRIDA_INT2_MASK  0x00000003U
#define FRIDA_INT11_MASK 0x000007ffU
#define FRIDA_INT16_MASK 0x0000ffffU
#define FRIDA_INT32_MASK 0xffffffffU

typedef uint8_t FridaUploadCommandType;
typedef uint8_t FridaDarwinThreadedItemType;

typedef void (* FridaConstructorFunc) (int argc, const char * argv[], const char * env[], const char * apple[], int * result);

typedef struct _FridaChainedFixupsHeader FridaChainedFixupsHeader;

typedef struct _FridaChainedStartsInImage FridaChainedStartsInImage;
typedef struct _FridaChainedStartsInSegment FridaChainedStartsInSegment;
typedef uint16_t FridaChainedPtrFormat;

typedef struct _FridaChainedPtr64Rebase FridaChainedPtr64Rebase;
typedef struct _FridaChainedPtr64Bind FridaChainedPtr64Bind;
typedef struct _FridaChainedPtrArm64eRebase FridaChainedPtrArm64eRebase;
typedef struct _FridaChainedPtrArm64eBind FridaChainedPtrArm64eBind;
typedef struct _FridaChainedPtrArm64eBind24 FridaChainedPtrArm64eBind24;
typedef struct _FridaChainedPtrArm64eAuthRebase FridaChainedPtrArm64eAuthRebase;
typedef struct _FridaChainedPtrArm64eAuthBind FridaChainedPtrArm64eAuthBind;
typedef struct _FridaChainedPtrArm64eAuthBind24 FridaChainedPtrArm64eAuthBind24;

typedef uint32_t FridaChainedImportFormat;
typedef uint32_t FridaChainedSymbolFormat;

typedef struct _FridaChainedImport FridaChainedImport;
typedef struct _FridaChainedImportAddend FridaChainedImportAddend;
typedef struct _FridaChainedImportAddend64 FridaChainedImportAddend64;

enum _FridaUploadCommandType
{
  FRIDA_UPLOAD_COMMAND_WRITE = 1,
  FRIDA_UPLOAD_COMMAND_APPLY_THREADED,
  FRIDA_UPLOAD_COMMAND_PROCESS_FIXUPS,
  FRIDA_UPLOAD_COMMAND_PROTECT,
  FRIDA_UPLOAD_COMMAND_CONSTRUCT_FROM_POINTERS,
  FRIDA_UPLOAD_COMMAND_CONSTRUCT_FROM_OFFSETS,
  FRIDA_UPLOAD_COMMAND_CHECK,
};

enum _FridaDarwinThreadedItemType
{
  FRIDA_DARWIN_THREADED_REBASE,
  FRIDA_DARWIN_THREADED_BIND
};

struct _FridaChainedFixupsHeader
{
  uint32_t fixups_version;
  uint32_t starts_offset;
  uint32_t imports_offset;
  uint32_t symbols_offset;
  uint32_t imports_count;
  FridaChainedImportFormat imports_format;
  FridaChainedSymbolFormat symbols_format;
};

struct _FridaChainedStartsInImage
{
  uint32_t seg_count;
  uint32_t seg_info_offset[1];
};

struct _FridaChainedStartsInSegment
{
  uint32_t size;
  uint16_t page_size;
  FridaChainedPtrFormat pointer_format;
  uint64_t segment_offset;
  uint32_t max_valid_pointer;
  uint16_t page_count;
  uint16_t page_start[1];
};

enum _FridaChainedPtrStart
{
  FRIDA_CHAINED_PTR_START_NONE  = 0xffff,
  FRIDA_CHAINED_PTR_START_MULTI = 0x8000,
  FRIDA_CHAINED_PTR_START_LAST  = 0x8000,
};

enum _FridaChainedPtrFormat
{
  FRIDA_CHAINED_PTR_ARM64E              =  1,
  FRIDA_CHAINED_PTR_64                  =  2,
  FRIDA_CHAINED_PTR_32                  =  3,
  FRIDA_CHAINED_PTR_32_CACHE            =  4,
  FRIDA_CHAINED_PTR_32_FIRMWARE         =  5,
  FRIDA_CHAINED_PTR_64_OFFSET           =  6,
  FRIDA_CHAINED_PTR_ARM64E_OFFSET       =  7,
  FRIDA_CHAINED_PTR_ARM64E_KERNEL       =  7,
  FRIDA_CHAINED_PTR_64_KERNEL_CACHE     =  8,
  FRIDA_CHAINED_PTR_ARM64E_USERLAND     =  9,
  FRIDA_CHAINED_PTR_ARM64E_FIRMWARE     = 10,
  FRIDA_CHAINED_PTR_X86_64_KERNEL_CACHE = 11,
  FRIDA_CHAINED_PTR_ARM64E_USERLAND24   = 12,
};

struct _FridaChainedPtr64Rebase
{
  uint64_t target   : 36,
           high8    :  8,
           reserved :  7,
           next     : 12,
           bind     :  1;
};

struct _FridaChainedPtr64Bind
{
  uint64_t ordinal  : 24,
           addend   :  8,
           reserved : 19,
           next     : 12,
           bind     :  1;
};

struct _FridaChainedPtrArm64eRebase
{
  uint64_t target : 43,
           high8  :  8,
           next   : 11,
           bind   :  1,
           auth   :  1;
};

struct _FridaChainedPtrArm64eBind
{
  uint64_t ordinal : 16,
           zero    : 16,
           addend  : 19,
           next    : 11,
           bind    :  1,
           auth    :  1;
};

struct _FridaChainedPtrArm64eBind24
{
  uint64_t ordinal : 24,
           zero    :  8,
           addend  : 19,
           next    : 11,
           bind    :  1,
           auth    :  1;
};

struct _FridaChainedPtrArm64eAuthRebase
{
  uint64_t target    : 32,
           diversity : 16,
           addr_div  :  1,
           key       :  2,
           next      : 11,
           bind      :  1,
           auth      :  1;
};

struct _FridaChainedPtrArm64eAuthBind
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

struct _FridaChainedPtrArm64eAuthBind24
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

enum _FridaChainedImportFormat
{
  FRIDA_CHAINED_IMPORT          = 1,
  FRIDA_CHAINED_IMPORT_ADDEND   = 2,
  FRIDA_CHAINED_IMPORT_ADDEND64 = 3,
};

enum _FridaChainedSymbolFormat
{
  FRIDA_CHAINED_SYMBOL_UNCOMPRESSED,
  FRIDA_CHAINED_SYMBOL_ZLIB_COMPRESSED,
};

struct _FridaChainedImport
{
  uint32_t lib_ordinal :  8,
           weak_import :  1,
           name_offset : 23;
};

struct _FridaChainedImportAddend
{
  uint32_t lib_ordinal :  8,
           weak_import :  1,
           name_offset : 23;
  int32_t  addend;
};

struct _FridaChainedImportAddend64
{
  uint64_t lib_ordinal : 16,
           weak_import :  1,
           reserved    : 15,
           name_offset : 32;
  uint64_t addend;
};

#define FRIDA_TEMP_FAILURE_RETRY(expression) \
  ({ \
    ssize_t __result; \
    \
    do __result = expression; \
    while (__result == -1 && *(api->get_errno_storage ()) == EINTR); \
    \
    __result; \
  })

static void frida_apply_threaded_items (uint64_t preferred_base_address, uint64_t slide, uint16_t num_symbols, const uint64_t * symbols,
    uint16_t num_regions, uint64_t * regions);

static void frida_process_chained_fixups (const FridaChainedFixupsHeader * fixups_header, struct mach_header_64 * mach_header,
    size_t preferred_base_address, const FridaUploadApi * api);
static void frida_process_chained_fixups_in_segment_generic64 (void * cursor, FridaChainedPtrFormat format, uint64_t actual_base_address,
    uint64_t preferred_base_address, void ** bound_pointers);
static void frida_process_chained_fixups_in_segment_arm64e (void * cursor, FridaChainedPtrFormat format, uint64_t actual_base_address,
    uint64_t preferred_base_address, void ** bound_pointers);
static void * frida_resolve_import (void ** dylib_handles, int dylib_ordinal, const char * symbol_strings, uint32_t symbol_offset,
    const FridaUploadApi * api);

static void * frida_sign_pointer (void * ptr, uint8_t key, uintptr_t diversity, bool use_address_diversity, void * address_of_ptr);
static const char * frida_symbol_name_from_darwin (const char * name);
static int64_t frida_sign_extend_int19 (uint64_t i19);

static bool frida_read_chunk (int fd, void * buffer, size_t length, size_t * bytes_read, const FridaUploadApi * api);
static bool frida_write_chunk (int fd, const void * buffer, size_t length, size_t * bytes_written, const FridaUploadApi * api);

int64_t
frida_receive (int listener_fd, uint64_t session_id_top, uint64_t session_id_bottom, const char * apple[], const FridaUploadApi * api)
{
  int result = 0;
  mach_port_t task;
  bool expecting_client;
  int res;
  struct sockaddr_in addr;
  socklen_t addr_len;
  int client_fd;
  uint32_t ACK_MAGIC = 0xac4ac4ac;

  task = api->_mach_task_self ();

  expecting_client = true;

  do
  {
    uint64_t client_sid[2];

    addr_len = sizeof (addr);

    res = FRIDA_TEMP_FAILURE_RETRY (api->accept (listener_fd, (struct sockaddr *) &addr, &addr_len));
    if (res == -1)
      goto beach;
    client_fd = res;

    #define FRIDA_READ_VALUE(v) \
        if (!frida_read_chunk (client_fd, &(v), sizeof (v), NULL, api)) \
          goto next_client

    #define FRIDA_WRITE_VALUE(v) \
        if (!frida_write_chunk (client_fd, &(v), sizeof (v), NULL, api)) \
          goto next_client

    FRIDA_READ_VALUE (client_sid);
    if (client_sid[0] != session_id_top || client_sid[1] != session_id_bottom)
      goto next_client;

    expecting_client = false;

    FRIDA_WRITE_VALUE (ACK_MAGIC);

    while (true)
    {
      bool success = false;
      FridaUploadCommandType command_type;

      FRIDA_READ_VALUE (command_type);

      switch (command_type)
      {
        case FRIDA_UPLOAD_COMMAND_WRITE:
        {
          uint64_t address;
          uint32_t size;
          vm_address_t writable_address;
          vm_prot_t cur_prot, max_prot;
          size_t n;

          FRIDA_READ_VALUE (address);
          FRIDA_READ_VALUE (size);

          writable_address = 0;
          success = api->vm_remap (task, &writable_address, size, 0, VM_FLAGS_ANYWHERE, task, address, FALSE, &cur_prot, &max_prot,
              VM_INHERIT_NONE) == 0;
          if (!success)
            break;

          success = api->mprotect ((void *) writable_address, size, VM_PROT_READ | VM_PROT_WRITE) == 0;
          if (!success)
            goto unmap_writable;

          success = frida_read_chunk (client_fd, (void *) writable_address, size, &n, api);
          if (!success)
            goto unmap_writable;

          api->sys_icache_invalidate ((void *) address, n);
          api->sys_dcache_flush ((void *) address, n);

unmap_writable:
          api->mach_vm_deallocate (task, writable_address, size);

          break;
        }
        case FRIDA_UPLOAD_COMMAND_APPLY_THREADED:
        {
          uint64_t preferred_base_address, slide;
          uint16_t num_symbols, num_regions;

          FRIDA_READ_VALUE (preferred_base_address);
          FRIDA_READ_VALUE (slide);

          FRIDA_READ_VALUE (num_symbols);
          uint64_t symbols[num_symbols];
          if (!frida_read_chunk (client_fd, symbols, num_symbols * sizeof (uint64_t), NULL, api))
            goto next_client;

          FRIDA_READ_VALUE (num_regions);
          uint64_t regions[num_regions];
          if (!frida_read_chunk (client_fd, regions, num_regions * sizeof (uint64_t), NULL, api))
            goto next_client;

          frida_apply_threaded_items (preferred_base_address, slide, num_symbols, symbols, num_regions, regions);

          success = true;

          break;
        }
        case FRIDA_UPLOAD_COMMAND_PROCESS_FIXUPS:
        {
          uint64_t fixups_header_address, mach_header_address, preferred_base_address;

          FRIDA_READ_VALUE (fixups_header_address);
          FRIDA_READ_VALUE (mach_header_address);
          FRIDA_READ_VALUE (preferred_base_address);

          frida_process_chained_fixups ((const FridaChainedFixupsHeader *) fixups_header_address,
              (struct mach_header_64 *) mach_header_address, (size_t) preferred_base_address, api);

          success = true;

          break;
        }
        case FRIDA_UPLOAD_COMMAND_PROTECT:
        {
          uint64_t address;
          uint32_t size;
          int32_t prot;

          FRIDA_READ_VALUE (address);
          FRIDA_READ_VALUE (size);
          FRIDA_READ_VALUE (prot);

          success = api->mprotect ((void *) address, size, prot) == 0;

          break;
        }
        case FRIDA_UPLOAD_COMMAND_CONSTRUCT_FROM_POINTERS:
        {
          uint64_t address;
          uint32_t count;
          FridaConstructorFunc * constructors;
          uint32_t i;

          FRIDA_READ_VALUE (address);
          FRIDA_READ_VALUE (count);

          constructors = (FridaConstructorFunc *) address;

          for (i = 0; i != count; i++)
          {
            const int argc = 0;
            const char * argv[] = { NULL };
            const char * env[] = { NULL };

            constructors[i] (argc, argv, env, apple, &result);
          }

          success = true;

          break;
        }
        case FRIDA_UPLOAD_COMMAND_CONSTRUCT_FROM_OFFSETS:
        {
          uint64_t address;
          uint32_t count;
          uint64_t mach_header_address;
          uint32_t * constructor_offsets;
          uint32_t i;

          FRIDA_READ_VALUE (address);
          FRIDA_READ_VALUE (count);
          FRIDA_READ_VALUE (mach_header_address);

          constructor_offsets = (uint32_t *) address;

          for (i = 0; i != count; i++)
          {
            FridaConstructorFunc constructor;
            const int argc = 0;
            const char * argv[] = { NULL };
            const char * env[] = { NULL };

            constructor = (FridaConstructorFunc) (mach_header_address + constructor_offsets[i]);

            constructor (argc, argv, env, apple, &result);
          }

          success = true;

          break;
        }
        case FRIDA_UPLOAD_COMMAND_CHECK:
        {
          FRIDA_WRITE_VALUE (ACK_MAGIC);

          success = true;

          break;
        }
      }

      if (!success)
        goto next_client;
    }

next_client:
    api->close (client_fd);
  }
  while (expecting_client);

beach:
  api->close (listener_fd);

#ifndef BUILDING_TEST_PROGRAM
  asm volatile (
      "mov x0, %0\n\t"
      "mov x1, #1337\n\t"
      "mov x2, #1337\n\t"
      "mov x3, #0\n\t"
      "brk #1337\n\t"
      :
      : "r" ((uint64_t) result)
      : "x0", "x1", "x2", "x3"
  );
#endif

  return result;
}

static void
frida_apply_threaded_items (uint64_t preferred_base_address, uint64_t slide, uint16_t num_symbols, const uint64_t * symbols,
    uint16_t num_regions, uint64_t * regions)
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
      FridaDarwinThreadedItemType type;
      uint8_t key;
      bool has_address_diversity;
      uint16_t diversity;
      uint64_t bound_value;

      value = *slot;

      is_authenticated      = (value >> 63) & 1;
      type                  = (value >> 62) & 1;
      delta                 = (value >> 51) & FRIDA_INT11_MASK;
      key                   = (value >> 49) & FRIDA_INT2_MASK;
      has_address_diversity = (value >> 48) & 1;
      diversity             = (value >> 32) & FRIDA_INT16_MASK;

      if (type == FRIDA_DARWIN_THREADED_BIND)
      {
        uint16_t bind_ordinal;

        bind_ordinal = value & FRIDA_INT16_MASK;

        bound_value = symbols[bind_ordinal];
      }
      else if (type == FRIDA_DARWIN_THREADED_REBASE)
      {
        uint64_t rebase_address;

        if (is_authenticated)
        {
          rebase_address = value & FRIDA_INT32_MASK;
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
        *slot = (uint64_t) frida_sign_pointer ((void *) bound_value, key, diversity, has_address_diversity, slot);
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

static void
frida_process_chained_fixups (const FridaChainedFixupsHeader * fixups_header, struct mach_header_64 * mach_header,
    size_t preferred_base_address, const FridaUploadApi * api)
{
  mach_port_t task;
  mach_vm_address_t slab_start;
  size_t slab_size;
  void * slab_cursor;
  void ** dylib_handles;
  size_t dylib_count;
  const void * command;
  uint32_t command_index;
  void ** bound_pointers;
  size_t bound_count, i;
  const char * symbols;
  const FridaChainedStartsInImage * image_starts;
  uint32_t seg_index;

  task = api->_mach_task_self ();

  slab_start = 0;
  slab_size = 64 * 1024;
  api->mach_vm_allocate (task, &slab_start, slab_size, VM_FLAGS_ANYWHERE);
  slab_cursor = (void *) slab_start;

  dylib_handles = slab_cursor;
  dylib_count = 0;

  command = mach_header + 1;
  for (command_index = 0; command_index != mach_header->ncmds; command_index++)
  {
    const struct load_command * lc = command;

    switch (lc->cmd)
    {
      case LC_LOAD_DYLIB:
      case LC_LOAD_WEAK_DYLIB:
      case LC_REEXPORT_DYLIB:
      case LC_LOAD_UPWARD_DYLIB:
      {
        const struct dylib_command * dc = command;
        const char * name = command + dc->dylib.name.offset;

        dylib_handles[dylib_count++] = api->dlopen (name, RTLD_LAZY | RTLD_GLOBAL);

        break;
      }
      default:
        break;
    }

    command += lc->cmdsize;
  }

  slab_cursor += dylib_count * sizeof (void *);

  bound_pointers = slab_cursor;
  bound_count = fixups_header->imports_count;
  slab_cursor += bound_count * sizeof (void *);

  symbols = (const char *) fixups_header + fixups_header->symbols_offset;

  switch (fixups_header->imports_format)
  {
    case FRIDA_CHAINED_IMPORT:
    {
      const FridaChainedImport * imports = ((const void *) fixups_header + fixups_header->imports_offset);

      for (i = 0; i != bound_count; i++)
      {
        const FridaChainedImport * import = &imports[i];

        bound_pointers[i] = frida_resolve_import (dylib_handles,
            import->lib_ordinal, symbols, import->name_offset, api);
      }

      break;
    }
    case FRIDA_CHAINED_IMPORT_ADDEND:
    {
      const FridaChainedImportAddend * imports = ((const void *) fixups_header + fixups_header->imports_offset);

      for (i = 0; i != bound_count; i++)
      {
        const FridaChainedImportAddend * import = &imports[i];

        bound_pointers[i] = frida_resolve_import (dylib_handles,
            import->lib_ordinal, symbols, import->name_offset, api);
        bound_pointers[i] += import->addend;
      }

      break;
    }
    case FRIDA_CHAINED_IMPORT_ADDEND64:
    {
      const FridaChainedImportAddend64 * imports = ((const void *) fixups_header + fixups_header->imports_offset);

      for (i = 0; i != bound_count; i++)
      {
        const FridaChainedImportAddend64 * import = &imports[i];

        bound_pointers[i] = frida_resolve_import (dylib_handles,
            import->lib_ordinal, symbols, import->name_offset, api);
        bound_pointers[i] += import->addend;
      }

      break;
    }
  }

  image_starts = (const FridaChainedStartsInImage *) ((const void *) fixups_header + fixups_header->starts_offset);

  for (seg_index = 0; seg_index != image_starts->seg_count; seg_index++)
  {
    const uint32_t seg_offset = image_starts->seg_info_offset[seg_index];
    const FridaChainedStartsInSegment * seg_starts;
    FridaChainedPtrFormat format;
    uint16_t page_index;

    if (seg_offset == 0)
      continue;

    seg_starts = (const FridaChainedStartsInSegment *) ((const void *) image_starts + seg_offset);
    format = seg_starts->pointer_format;

    for (page_index = 0; page_index != seg_starts->page_count; page_index++)
    {
      uint16_t start;
      void * cursor;

      start = seg_starts->page_start[page_index];
      if (start == FRIDA_CHAINED_PTR_START_NONE)
        continue;
      /* Ignoring MULTI for now as it only applies to 32-bit formats. */

      cursor = (void *) mach_header + seg_starts->segment_offset + (page_index * seg_starts->page_size) + start;

      if (format == FRIDA_CHAINED_PTR_64 || format == FRIDA_CHAINED_PTR_64_OFFSET)
      {
        frida_process_chained_fixups_in_segment_generic64 (cursor, format, (uintptr_t) mach_header, preferred_base_address, bound_pointers);
      }
      else
      {
        frida_process_chained_fixups_in_segment_arm64e (cursor, format, (uintptr_t) mach_header, preferred_base_address, bound_pointers);
      }
    }
  }

  api->mach_vm_deallocate (task, slab_start, slab_size);
}

static void
frida_process_chained_fixups_in_segment_generic64 (void * cursor, FridaChainedPtrFormat format, uint64_t actual_base_address,
    uint64_t preferred_base_address, void ** bound_pointers)
{
  const int64_t slide = actual_base_address - preferred_base_address;
  const size_t stride = 4;

  while (TRUE)
  {
    uint64_t * slot = cursor;
    size_t delta;

    if ((*slot >> 63) == 0)
    {
      FridaChainedPtr64Rebase * item = cursor;
      uint64_t top_8_bits, bottom_36_bits, unpacked_target;

      delta = item->next;

      top_8_bits = (uint64_t) item->high8 << (64 - 8);
      bottom_36_bits = item->target;
      unpacked_target = top_8_bits | bottom_36_bits;

      if (format == FRIDA_CHAINED_PTR_64_OFFSET)
        *slot = actual_base_address + unpacked_target;
      else
        *slot = unpacked_target + slide;
    }
    else
    {
      FridaChainedPtr64Bind * item = cursor;

      delta = item->next;

      *slot = (uint64_t) (bound_pointers[item->ordinal] + item->addend);
    }

    if (delta == 0)
      break;

    cursor += delta * stride;
  }
}

static void
frida_process_chained_fixups_in_segment_arm64e (void * cursor, FridaChainedPtrFormat format, uint64_t actual_base_address,
    uint64_t preferred_base_address, void ** bound_pointers)
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
        FridaChainedPtrArm64eRebase * item = cursor;
        uint64_t top_8_bits, bottom_43_bits, unpacked_target;

        delta = item->next;

        top_8_bits = (uint64_t) item->high8 << (64 - 8);
        bottom_43_bits = item->target;

        unpacked_target = top_8_bits | bottom_43_bits;

        if (format == FRIDA_CHAINED_PTR_ARM64E)
          *slot = unpacked_target + slide;
        else
          *slot = actual_base_address + unpacked_target;

        break;
      }
      case 0b01:
      {
        FridaChainedPtrArm64eBind * item = cursor;
        FridaChainedPtrArm64eBind24 * item24 = cursor;
        uint32_t ordinal;

        delta = item->next;

        ordinal = (format == FRIDA_CHAINED_PTR_ARM64E_USERLAND24)
            ? item24->ordinal
            : item->ordinal;

        *slot = (uint64_t) (bound_pointers[ordinal] +
            frida_sign_extend_int19 (item->addend));

        break;
      }
      case 0b10:
      {
        FridaChainedPtrArm64eAuthRebase * item = cursor;

        delta = item->next;

        *slot = (uint64_t) frida_sign_pointer ((void *) (preferred_base_address + item->target + slide), item->key, item->diversity,
            item->addr_div, slot);

        break;
      }
      case 0b11:
      {
        FridaChainedPtrArm64eAuthBind * item = cursor;
        FridaChainedPtrArm64eAuthBind24 * item24 = cursor;
        uint32_t ordinal;

        delta = item->next;

        ordinal = (format == FRIDA_CHAINED_PTR_ARM64E_USERLAND24)
            ? item24->ordinal
            : item->ordinal;

        *slot = (uint64_t) frida_sign_pointer (bound_pointers[ordinal], item->key, item->diversity, item->addr_div, slot);

        break;
      }
    }

    if (delta == 0)
      break;

    cursor += delta * stride;
  }
}

static void *
frida_resolve_import (void ** dylib_handles, int dylib_ordinal, const char * symbol_strings, uint32_t symbol_offset,
    const FridaUploadApi * api)
{
  void * result;
  const char * raw_name, * name;

  if (dylib_ordinal <= 0)
    return NULL; /* Placeholder if we ever need to support this. */

  raw_name = symbol_strings + symbol_offset;
  name = frida_symbol_name_from_darwin (raw_name);

  result = api->dlsym (dylib_handles[dylib_ordinal - 1], name);

  result = ptrauth_strip (result, ptrauth_key_asia);

  return result;
}

static void *
frida_sign_pointer (void * ptr, uint8_t key, uintptr_t diversity, bool use_address_diversity, void * address_of_ptr)
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

static const char *
frida_symbol_name_from_darwin (const char * name)
{
  return (name[0] == '_') ? name + 1 : name;
}

static int64_t
frida_sign_extend_int19 (uint64_t i19)
{
  int64_t result;
  bool sign_bit_set;

  result = i19;

  sign_bit_set = i19 >> (19 - 1);
  if (sign_bit_set)
    result |= 0xfffffffffff80000ULL;

  return result;
}

static bool
frida_read_chunk (int fd, void * buffer, size_t length, size_t * bytes_read, const FridaUploadApi * api)
{
  void * cursor = buffer;
  size_t remaining = length;

  if (bytes_read != NULL)
    *bytes_read = 0;

  while (remaining != 0)
  {
    ssize_t n;

    n = FRIDA_TEMP_FAILURE_RETRY (api->read (fd, cursor, remaining));
    if (n <= 0)
      return false;

    if (bytes_read != NULL)
      *bytes_read += n;

    cursor += n;
    remaining -= n;
  }

  return true;
}

static bool
frida_write_chunk (int fd, const void * buffer, size_t length, size_t * bytes_written, const FridaUploadApi * api)
{
  const void * cursor = buffer;
  size_t remaining = length;

  if (bytes_written != NULL)
    *bytes_written = 0;

  while (remaining != 0)
  {
    ssize_t n;

    n = FRIDA_TEMP_FAILURE_RETRY (api->write (fd, cursor, remaining));
    if (n <= 0)
      return false;

    if (bytes_written != NULL)
      *bytes_written += n;

    cursor += n;
    remaining -= n;
  }

  return true;
}

#ifdef BUILDING_TEST_PROGRAM

#include <assert.h>
#include <pthread.h>
#include <stdio.h>

# undef BUILDING_TEST_PROGRAM
# include "upload-listener.c"
# define BUILDING_TEST_PROGRAM
# undef FRIDA_WRITE_VALUE

typedef struct _FridaTestState FridaTestState;

struct _FridaTestState
{
  uint16_t port;

  uint64_t session_id_top;
  uint64_t session_id_bottom;

  uint8_t target_a[4];
  uint8_t target_b[2];

  const FridaUploadApi * api;
};

static void * frida_emulate_client (void * user_data);

int
main (void)
{
  const FridaUploadApi api = FRIDA_UPLOAD_API_INIT;
  uint64_t result;
  uint8_t error_code;
  uint32_t listener_fd;
  uint16_t port;
  pthread_t client_thread;
  FridaTestState state;
  const char * apple[] = { NULL };

  result = frida_listen (FRIDA_RX_BUFFER_SIZE, &api);

  error_code  = (result >> 56) & 0xff;
  listener_fd = (result >> 16) & 0xffffffff;
  port        =  result        & 0xffff;

  printf ("listen() => error_code=%u fd=%u port=%u\n", error_code, listener_fd, port);

  assert (error_code == 0);

  state.port = port;

  state.session_id_top = 1;
  state.session_id_bottom = 2;

  state.target_a[0] = 0;
  state.target_a[1] = 0;
  state.target_a[2] = 3;
  state.target_a[3] = 4;
  state.target_b[0] = 0;
  state.target_b[1] = 6;

  state.api = &api;

  pthread_create (&client_thread, NULL, frida_emulate_client, &state);

  frida_receive (listener_fd, 1, 2, apple, &api);

  pthread_join (client_thread, NULL);

  assert (state.target_a[0] == 1);
  assert (state.target_a[1] == 2);
  assert (state.target_a[2] == 3);
  assert (state.target_a[3] == 4);
  assert (state.target_b[0] == 5);
  assert (state.target_b[1] == 6);

  return 0;
}

static void *
frida_emulate_client (void * user_data)
{
  FridaTestState * state = user_data;
  const FridaUploadApi * api = state->api;
  struct sockaddr_in addr;
  int fd;
  int res;
  bool success;
  const FridaUploadCommandType write_command_type = FRIDA_UPLOAD_COMMAND_WRITE;
  uint64_t address;
  uint32_t size;
  uint8_t val_a[2], val_b;

  fd = api->socket (AF_INET, SOCK_STREAM, 0);
  assert (fd != -1);

  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
  addr.sin_port = htons (state->port);

  res = FRIDA_TEMP_FAILURE_RETRY (connect (fd, (const struct sockaddr *) &addr, sizeof (addr)));
  assert (res != -1);

  #define FRIDA_WRITE_VALUE(v) \
      success = frida_write_chunk (fd, &(v), sizeof (v), NULL, api); \
      assert (success)

  FRIDA_WRITE_VALUE (state->session_id_top);
  FRIDA_WRITE_VALUE (state->session_id_bottom);

  FRIDA_WRITE_VALUE (write_command_type);
  address = (uint64_t) &state->target_a;
  FRIDA_WRITE_VALUE (address);
  size = 2;
  FRIDA_WRITE_VALUE (size);
  val_a[0] = 1;
  val_a[1] = 2;
  FRIDA_WRITE_VALUE (val_a);

  FRIDA_WRITE_VALUE (write_command_type);
  address = (uint64_t) &state->target_b;
  FRIDA_WRITE_VALUE (address);
  size = 1;
  FRIDA_WRITE_VALUE (size);
  val_b = 5;
  FRIDA_WRITE_VALUE (val_b);

  api->close (fd);

  return NULL;
}

#endif
