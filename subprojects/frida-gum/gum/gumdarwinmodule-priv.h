/*
 * Copyright (C) 2015-2025 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2022 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2023 Fabian Freyer <fabian.freyer@physik.tu-berlin.de>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_DARWIN_MODULE_PRIV_H__
#define __GUM_DARWIN_MODULE_PRIV_H__

#include "gumdarwinmodule.h"

#define GUM_FAT_CIGAM_32               0xbebafeca
#define GUM_MH_MAGIC_32                0xfeedface
#define GUM_MH_MAGIC_64                0xfeedfacf
#define GUM_MH_EXECUTE                        0x2
#define GUM_MH_PREBOUND                      0x10
#define GUM_MH_HAS_TLV_DESCRIPTORS       0x800000

#define GUM_LC_REQ_DYLD                0x80000000

#define GUM_SECTION_TYPE_MASK          0x000000ff

#define GUM_S_THREAD_LOCAL_REGULAR           0x11
#define GUM_S_THREAD_LOCAL_ZEROFILL          0x12
#define GUM_S_THREAD_LOCAL_VARIABLES         0x13

#define GUM_N_EXT                            0x01
#define GUM_N_TYPE                           0x0e
#define GUM_N_SECT                            0xe

#define GUM_REBASE_OPCODE_MASK               0xf0
#define GUM_REBASE_IMMEDIATE_MASK            0x0f

#define GUM_BIND_OPCODE_MASK                 0xf0
#define GUM_BIND_IMMEDIATE_MASK              0x0f

#define GUM_BIND_TYPE_POINTER 1

#define GUM_BIND_SPECIAL_DYLIB_SELF             0
#define GUM_BIND_SPECIAL_DYLIB_MAIN_EXECUTABLE -1
#define GUM_BIND_SPECIAL_DYLIB_FLAT_LOOKUP     -2
#define GUM_BIND_SPECIAL_DYLIB_WEAK_LOOKUP     -3

G_BEGIN_DECLS

typedef struct _GumFatHeader GumFatHeader;
typedef struct _GumFatArch32 GumFatArch32;
typedef struct _GumMachHeader32 GumMachHeader32;
typedef struct _GumMachHeader64 GumMachHeader64;
typedef struct _GumLoadCommand GumLoadCommand;
typedef union _GumLcStr GumLcStr;
typedef struct _GumSegmentCommand32 GumSegmentCommand32;
typedef struct _GumSegmentCommand64 GumSegmentCommand64;
typedef struct _GumDylibCommand GumDylibCommand;
typedef struct _GumDylinkerCommand GumDylinkerCommand;
typedef struct _GumUUIDCommand GumUUIDCommand;
typedef struct _GumSourceVersionCommand GumSourceVersionCommand;
typedef struct _GumDylib GumDylib;
typedef struct _GumLinkeditDataCommand GumLinkeditDataCommand;
typedef struct _GumSection32 GumSection32;
typedef struct _GumSection64 GumSection64;
typedef struct _GumNList32 GumNList32;
typedef struct _GumNList64 GumNList64;
typedef struct _GumTlvThunk32 GumTlvThunk32;
typedef struct _GumTlvThunk64 GumTlvThunk64;

struct _GumFatHeader
{
  guint32 magic;
  guint32 nfat_arch;
};

struct _GumFatArch32
{
  GumDarwinCpuType cputype;
  GumDarwinCpuSubtype cpusubtype;
  guint32 offset;
  guint32 size;
  guint32 align;
};

struct _GumMachHeader32
{
  guint32 magic;
  GumDarwinCpuType cputype;
  GumDarwinCpuSubtype cpusubtype;
  guint32 filetype;
  guint32 ncmds;
  guint32 sizeofcmds;
  guint32 flags;
};

struct _GumMachHeader64
{
  guint32 magic;
  GumDarwinCpuType cputype;
  GumDarwinCpuSubtype cpusubtype;
  guint32 filetype;
  guint32 ncmds;
  guint32 sizeofcmds;
  guint32 flags;
  guint32 reserved;
};

enum _GumLoadCommandType
{
  GUM_LC_SEGMENT_32               = 0x01,
  GUM_LC_SYMTAB                   = 0x02,
  GUM_LC_DYSYMTAB                 = 0x0b,
  GUM_LC_LOAD_DYLIB               = 0x0c,
  GUM_LC_ID_DYLIB                 = 0x0d,
  GUM_LC_ID_DYLINKER              = 0x0f,
  GUM_LC_LOAD_WEAK_DYLIB          = (0x18 | GUM_LC_REQ_DYLD),
  GUM_LC_SEGMENT_64               = 0x19,
  GUM_LC_UUID                     = 0x1b,
  GUM_LC_CODE_SIGNATURE           = 0x1d,
  GUM_LC_SEGMENT_SPLIT_INFO       = 0x1e,
  GUM_LC_REEXPORT_DYLIB           = (0x1f | GUM_LC_REQ_DYLD),
  GUM_LC_DYLD_INFO_ONLY           = (0x22 | GUM_LC_REQ_DYLD),
  GUM_LC_LOAD_UPWARD_DYLIB        = (0x23 | GUM_LC_REQ_DYLD),
  GUM_LC_FUNCTION_STARTS          = 0x26,
  GUM_LC_DATA_IN_CODE             = 0x29,
  GUM_LC_SOURCE_VERSION           = 0x2a,
  GUM_LC_DYLIB_CODE_SIGN_DRS      = 0x2b,
  GUM_LC_LINKER_OPTIMIZATION_HINT = 0x2e,
  GUM_LC_DYLD_EXPORTS_TRIE        = (0x33 | GUM_LC_REQ_DYLD),
  GUM_LC_DYLD_CHAINED_FIXUPS      = (0x34 | GUM_LC_REQ_DYLD),
};

struct _GumLoadCommand
{
  guint32 cmd;
  guint32 cmdsize;
};

union _GumLcStr
{
  guint32 offset;
};

struct _GumSegmentCommand32
{
  guint32 cmd;
  guint32 cmdsize;

  gchar segname[16];

  guint32 vmaddr;
  guint32 vmsize;

  guint32 fileoff;
  guint32 filesize;

  GumDarwinPageProtection maxprot;
  GumDarwinPageProtection initprot;

  guint32 nsects;

  guint32 flags;
};

struct _GumSegmentCommand64
{
  guint32 cmd;
  guint32 cmdsize;

  gchar segname[16];

  guint64 vmaddr;
  guint64 vmsize;

  guint64 fileoff;
  guint64 filesize;

  GumDarwinPageProtection maxprot;
  GumDarwinPageProtection initprot;

  guint32 nsects;

  guint32 flags;
};

enum _GumVMProt
{
  GUM_VM_PROT_NONE    = 0,
  GUM_VM_PROT_READ    = (1 << 0),
  GUM_VM_PROT_WRITE   = (1 << 1),
  GUM_VM_PROT_EXECUTE = (1 << 2),
};

struct _GumDylib
{
  GumLcStr name;
  guint32 timestamp;
  guint32 current_version;
  guint32 compatibility_version;
};

struct _GumDylibCommand
{
  guint32 cmd;
  guint32 cmdsize;

  GumDylib dylib;
};

struct _GumDylinkerCommand
{
  guint32 cmd;
  guint32 cmdsize;

  GumLcStr name;
};

struct _GumUUIDCommand
{
  guint32 cmd;
  guint32 cmdsize;

  guint8 uuid[16];
};

struct _GumSourceVersionCommand
{
  guint32 cmd;
  guint32 cmdsize;

  guint64 version;
};

struct _GumDyldInfoCommand
{
  guint32 cmd;
  guint32 cmdsize;

  guint32 rebase_off;
  guint32 rebase_size;

  guint32 bind_off;
  guint32 bind_size;

  guint32 weak_bind_off;
  guint32 weak_bind_size;

  guint32 lazy_bind_off;
  guint32 lazy_bind_size;

  guint32 export_off;
  guint32 export_size;
};

struct _GumSymtabCommand
{
  guint32 cmd;
  guint32 cmdsize;

  guint32 symoff;
  guint32 nsyms;

  guint32 stroff;
  guint32 strsize;
};

struct _GumDysymtabCommand
{
  guint32 cmd;
  guint32 cmdsize;

  guint32 ilocalsym;
  guint32 nlocalsym;

  guint32 iextdefsym;
  guint32 nextdefsym;

  guint32 iundefsym;
  guint32 nundefsym;

  guint32 tocoff;
  guint32 ntoc;

  guint32 modtaboff;
  guint32 nmodtab;

  guint32 extrefsymoff;
  guint32 nextrefsyms;

  guint32 indirectsymoff;
  guint32 nindirectsyms;

  guint32 extreloff;
  guint32 nextrel;

  guint32 locreloff;
  guint32 nlocrel;
};

struct _GumLinkeditDataCommand
{
  guint32 cmd;
  guint32 cmdsize;

  guint32 dataoff;
  guint32 datasize;
};

enum _GumSectionType
{
  GUM_S_MOD_INIT_FUNC_POINTERS = 0x09,
  GUM_S_MOD_TERM_FUNC_POINTERS = 0x0a,
  GUM_S_INIT_FUNC_OFFSETS      = 0x16,
};

enum _GumSectionAttributes
{
  GUM_S_ATTR_SOME_INSTRUCTIONS = 0x00000400,
  GUM_S_ATTR_PURE_INSTRUCTIONS = 0x80000000,
};

struct _GumSection32
{
  gchar sectname[16];
  gchar segname[16];
  guint32 addr;
  guint32 size;
  guint32 offset;
  guint32 align;
  guint32 reloff;
  guint32 nreloc;
  guint32 flags;
  guint32 reserved1;
  guint32 reserved2;
};

struct _GumSection64
{
  gchar sectname[16];
  gchar segname[16];
  guint64 addr;
  guint64 size;
  guint32 offset;
  guint32 align;
  guint32 reloff;
  guint32 nreloc;
  guint32 flags;
  guint32 reserved1;
  guint32 reserved2;
  guint32 reserved3;
};

struct _GumNList32
{
  guint32 n_strx;
  guint8 n_type;
  guint8 n_sect;
  gint16 n_desc;
  guint32 n_value;
};

struct _GumNList64
{
  guint32 n_strx;
  guint8 n_type;
  guint8 n_sect;
  guint16 n_desc;
  guint64 n_value;
};

enum _GumRebaseOpcode
{
  GUM_REBASE_OPCODE_DONE                               = 0x00,
  GUM_REBASE_OPCODE_SET_TYPE_IMM                       = 0x10,
  GUM_REBASE_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB        = 0x20,
  GUM_REBASE_OPCODE_ADD_ADDR_ULEB                      = 0x30,
  GUM_REBASE_OPCODE_ADD_ADDR_IMM_SCALED                = 0x40,
  GUM_REBASE_OPCODE_DO_REBASE_IMM_TIMES                = 0x50,
  GUM_REBASE_OPCODE_DO_REBASE_ULEB_TIMES               = 0x60,
  GUM_REBASE_OPCODE_DO_REBASE_ADD_ADDR_ULEB            = 0x70,
  GUM_REBASE_OPCODE_DO_REBASE_ULEB_TIMES_SKIPPING_ULEB = 0x80,
};

enum _GumBindOpcode
{
  GUM_BIND_OPCODE_DONE                                 = 0x00,
  GUM_BIND_OPCODE_SET_DYLIB_ORDINAL_IMM                = 0x10,
  GUM_BIND_OPCODE_SET_DYLIB_ORDINAL_ULEB               = 0x20,
  GUM_BIND_OPCODE_SET_DYLIB_SPECIAL_IMM                = 0x30,
  GUM_BIND_OPCODE_SET_SYMBOL_TRAILING_FLAGS_IMM        = 0x40,
  GUM_BIND_OPCODE_SET_TYPE_IMM                         = 0x50,
  GUM_BIND_OPCODE_SET_ADDEND_SLEB                      = 0x60,
  GUM_BIND_OPCODE_SET_SEGMENT_AND_OFFSET_ULEB          = 0x70,
  GUM_BIND_OPCODE_ADD_ADDR_ULEB                        = 0x80,
  GUM_BIND_OPCODE_DO_BIND                              = 0x90,
  GUM_BIND_OPCODE_DO_BIND_ADD_ADDR_ULEB                = 0xa0,
  GUM_BIND_OPCODE_DO_BIND_ADD_ADDR_IMM_SCALED          = 0xb0,
  GUM_BIND_OPCODE_DO_BIND_ULEB_TIMES_SKIPPING_ULEB     = 0xc0,
  GUM_BIND_OPCODE_THREADED                             = 0xd0,
};

enum _GumBindSubopcode
{
  GUM_BIND_SUBOPCODE_THREADED_SET_BIND_ORDINAL_TABLE_SIZE_ULEB = 0x00,
  GUM_BIND_SUBOPCODE_THREADED_APPLY                            = 0x01,
};

enum _GumExportSymbolFlags
{
  GUM_EXPORT_SYMBOL_FLAGS_REEXPORT                     = 0x08,
  GUM_EXPORT_SYMBOL_FLAGS_STUB_AND_RESOLVER            = 0x10,
};

struct _GumTlvThunk32
{
  guint32 thunk;
  guint32 key;
  guint32 offset;
};

struct _GumTlvThunk64
{
  guint64 thunk;
  guint64 key;
  guint64 offset;
};

G_END_DECLS

#endif
