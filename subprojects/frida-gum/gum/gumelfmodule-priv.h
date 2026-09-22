/*
 * Copyright (C) 2023 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_ELF_MODULE_PRIV_H__
#define __GUM_ELF_MODULE_PRIV_H__

#include "gumelfmodule.h"

#define GUM_ELF_ST_BIND(v) ((v) >> 4)
#define GUM_ELF_ST_TYPE(v) ((v) & GUM_INT4_MASK)

#define GUM_STN_UNDEF 0

G_BEGIN_DECLS

typedef struct _GumElfIdentity GumElfIdentity;
typedef struct _GumElfEhdr GumElfEhdr;
typedef struct _GumElfEhdr32 GumElfEhdr32;
typedef struct _GumElfPhdr GumElfPhdr;
typedef struct _GumElfPhdr32 GumElfPhdr32;
typedef struct _GumElfShdr GumElfShdr;
typedef struct _GumElfShdr32 GumElfShdr32;
typedef struct _GumElfSym GumElfSym;
typedef struct _GumElfSym32 GumElfSym32;
typedef struct _GumElfDyn GumElfDyn;
typedef struct _GumElfDyn32 GumElfDyn32;

struct _GumElfIdentity
{
  guint32 magic;
  guint8 klass;
  guint8 data_encoding;
  guint8 version;
  guint8 os_abi;
  guint8 os_abi_version;
  guint8 padding[7];
};

typedef enum {
  GUM_ELF_CLASS_NONE,
  GUM_ELF_CLASS_32,
  GUM_ELF_CLASS_64,
} GumElfClass;

typedef enum {
  GUM_ELF_DATA_ENCODING_NONE,
  GUM_ELF_DATA_ENCODING_LSB,
  GUM_ELF_DATA_ENCODING_MSB,
} GumElfDataEncoding;

struct _GumElfEhdr
{
  GumElfIdentity identity;
  guint16 type;
  guint16 machine;
  guint32 version;
  guint64 entry;
  guint64 phoff;
  guint64 shoff;
  guint32 flags;
  guint16 ehsize;
  guint16 phentsize;
  guint16 phnum;
  guint16 shentsize;
  guint16 shnum;
  guint16 shstrndx;
};

struct _GumElfEhdr32
{
  GumElfIdentity identity;
  guint16 type;
  guint16 machine;
  guint32 version;
  guint32 entry;
  guint32 phoff;
  guint32 shoff;
  guint32 flags;
  guint16 ehsize;
  guint16 phentsize;
  guint16 phnum;
  guint16 shentsize;
  guint16 shnum;
  guint16 shstrndx;
};

struct _GumElfPhdr
{
  guint32 type;
  guint32 flags;
  guint64 offset;
  guint64 vaddr;
  guint64 paddr;
  guint64 filesz;
  guint64 memsz;
  guint64 align;
};

struct _GumElfPhdr32
{
  guint32 type;
  guint32 offset;
  guint32 vaddr;
  guint32 paddr;
  guint32 filesz;
  guint32 memsz;
  guint32 flags;
  guint32 align;
};

typedef enum {
  GUM_ELF_PHDR_NULL,
  GUM_ELF_PHDR_LOAD,
  GUM_ELF_PHDR_DYNAMIC,
  GUM_ELF_PHDR_INTERP,
  GUM_ELF_PHDR_NOTE,
  GUM_ELF_PHDR_SHLIB,
  GUM_ELF_PHDR_PHDR,
  GUM_ELF_PHDR_TLS,
  GUM_ELF_PHDR_GNU_EH_FRAME = 0x6474e550,
  GUM_ELF_PHDR_GNU_STACK    = 0x6474e551,
  GUM_ELF_PHDR_GNU_RELRO    = 0x6474e552,
  GUM_ELF_PHDR_GNU_PROPERTY = 0x6474e553,
  GUM_ELF_PHDR_SUNWBSS      = 0x6ffffffa,
  GUM_ELF_PHDR_SUNWSTACK    = 0x6ffffffb,
} GumElfPhdrType;

typedef enum {
  GUM_ELF_PHDR_X = (1 << 0),
  GUM_ELF_PHDR_W = (1 << 1),
  GUM_ELF_PHDR_R = (1 << 2),
} GumElfPhdrFlags;

struct _GumElfShdr
{
  guint32 name;
  guint32 type;
  guint64 flags;
  guint64 addr;
  guint64 offset;
  guint64 size;
  guint32 link;
  guint32 info;
  guint64 addralign;
  guint64 entsize;
};

struct _GumElfShdr32
{
  guint32 name;
  guint32 type;
  guint32 flags;
  guint32 addr;
  guint32 offset;
  guint32 size;
  guint32 link;
  guint32 info;
  guint32 addralign;
  guint32 entsize;
};

struct _GumElfDyn
{
  gint64 tag;
  guint64 val;
};

struct _GumElfDyn32
{
  gint32 tag;
  guint32 val;
};

struct _GumElfSym
{
  guint32 name;
  guint8 info;
  guint8 other;
  guint16 shndx;
  guint64 value;
  guint64 size;
};

struct _GumElfSym32
{
  guint32 name;
  guint32 value;
  guint32 size;
  guint8 info;
  guint8 other;
  guint16 shndx;
};

G_END_DECLS

#endif
