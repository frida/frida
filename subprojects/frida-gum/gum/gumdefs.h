/*
 * Copyright (C) 2008-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2023-2026 Håvard Sørbø <havard@hsorbo.no>
 * Copyright (C) 2024 Yannis Juglaret <yjuglaret@mozilla.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUMDEFS_H__
#define __GUMDEFS_H__

#include <capstone.h>
#include <glib.h>
#include <gum/gumenumtypes.h>

#if CS_API_MAJOR >= 6
# define CS_ARCH_ARM64 CS_ARCH_AARCH64
#endif

#if !defined (GUM_STATIC) && defined (G_OS_WIN32)
#  ifdef GUM_EXPORTS
#    define GUM_API __declspec(dllexport)
#  else
#    define GUM_API __declspec(dllimport)
#  endif
#else
#  define GUM_API
#endif

G_BEGIN_DECLS

#define GUM_ERROR gum_error_quark ()

typedef enum {
  GUM_ERROR_FAILED,
  GUM_ERROR_NOT_FOUND,
  GUM_ERROR_EXISTS,
  GUM_ERROR_PERMISSION_DENIED,
  GUM_ERROR_INVALID_ARGUMENT,
  GUM_ERROR_NOT_SUPPORTED,
  GUM_ERROR_INVALID_DATA,
} GumError;

typedef guint64 GumAddress;
#define GUM_ADDRESS(a) ((GumAddress) (guintptr) (a))
#define GUM_TYPE_ADDRESS (gum_address_get_type ())
typedef guint GumOS;
typedef guint GumCallingConvention;
typedef guint GumAbiType;
typedef guint GumCpuFeatures;
typedef guint GumInstructionEncoding;
typedef guint GumArgType;
typedef struct _GumArgument GumArgument;
typedef guint GumBranchHint;
typedef struct _GumIA32CpuContext GumIA32CpuContext;
typedef struct _GumX64CpuContext GumX64CpuContext;
typedef union _GumX86VectorReg GumX86VectorReg;
typedef struct _GumArmCpuContext GumArmCpuContext;
typedef union _GumArmVectorReg GumArmVectorReg;
typedef struct _GumArm64CpuContext GumArm64CpuContext;
typedef union _GumArm64VectorReg GumArm64VectorReg;
typedef struct _GumMipsCpuContext GumMipsCpuContext;
typedef guint GumRelocationScenario;

#if defined (_M_IX86) || defined (__i386__)
# define GUM_NATIVE_CPU GUM_CPU_IA32
# define GUM_DEFAULT_CS_ARCH CS_ARCH_X86
# define gum_cs_arch_register_native cs_arch_register_x86
/**
 * GUM_DEFAULT_CS_MODE: (skip)
 */
# define GUM_DEFAULT_CS_MODE CS_MODE_32
# define GUM_CPU_CONTEXT_HAS_OUT_OF_LINE_VECTORS 1
# define GUM_X86_XMM_REG_COUNT 8
# define GUM_FXSAVE_OFFSET_XMM 160
typedef GumIA32CpuContext GumCpuContext;
#elif defined (_M_X64) || defined (__x86_64__)
# define GUM_NATIVE_CPU GUM_CPU_AMD64
# define GUM_DEFAULT_CS_ARCH CS_ARCH_X86
# define gum_cs_arch_register_native cs_arch_register_x86
/**
 * GUM_DEFAULT_CS_MODE: (skip)
 */
# define GUM_DEFAULT_CS_MODE CS_MODE_64
# define GUM_CPU_CONTEXT_HAS_OUT_OF_LINE_VECTORS 1
# define GUM_X86_XMM_REG_COUNT 16
# define GUM_FXSAVE_OFFSET_XMM 160
typedef GumX64CpuContext GumCpuContext;
#elif defined (_M_ARM) || defined (__arm__)
# define GUM_NATIVE_CPU GUM_CPU_ARM
# define GUM_DEFAULT_CS_ARCH CS_ARCH_ARM
# define gum_cs_arch_register_native cs_arch_register_arm
/**
 * GUM_DEFAULT_CS_MODE: (skip)
 */
# define GUM_DEFAULT_CS_MODE \
    ((cs_mode) (CS_MODE_ARM | CS_MODE_V8 | GUM_DEFAULT_CS_ENDIAN))
# define GUM_PSR_T_BIT 0x20
typedef GumArmCpuContext GumCpuContext;
#elif defined (_M_ARM64) || defined (__aarch64__)
# define GUM_NATIVE_CPU GUM_CPU_ARM64
# define GUM_DEFAULT_CS_ARCH CS_ARCH_ARM64
# define gum_cs_arch_register_native cs_arch_register_arm64
/**
 * GUM_DEFAULT_CS_MODE: (skip)
 */
# define GUM_DEFAULT_CS_MODE GUM_DEFAULT_CS_ENDIAN
typedef GumArm64CpuContext GumCpuContext;
#elif defined (__mips__)
# define GUM_NATIVE_CPU GUM_CPU_MIPS
# define GUM_DEFAULT_CS_ARCH CS_ARCH_MIPS
# define gum_cs_arch_register_native cs_arch_register_mips
# if GLIB_SIZEOF_VOID_P == 4
/**
 * GUM_DEFAULT_CS_MODE: (skip)
 */
#  define GUM_DEFAULT_CS_MODE ((cs_mode) \
    (CS_MODE_MIPS32 | GUM_DEFAULT_CS_ENDIAN))
# else
/**
 * GUM_DEFAULT_CS_MODE: (skip)
 */
#  define GUM_DEFAULT_CS_MODE ((cs_mode) \
    (CS_MODE_MIPS64 | GUM_DEFAULT_CS_ENDIAN))
# endif
typedef GumMipsCpuContext GumCpuContext;
#else
# error Unsupported architecture.
#endif
/*
 * The only non-legacy big-endian configuration on 32-bit ARM systems is BE8.
 * In this configuration, whilst the data is in big-endian, the code stream is
 * still in little-endian. Since Capstone is disassembling the code stream, it
 * should work in little-endian even on BE8 systems. On big-endian 64-bit ARM
 * systems, the code stream is likewise in little-endian.
 */
#if G_BYTE_ORDER == G_LITTLE_ENDIAN || \
    defined (__arm__) || \
    defined (_M_ARM64) || \
    defined (__aarch64__)
# define GUM_DEFAULT_CS_ENDIAN CS_MODE_LITTLE_ENDIAN
#else
# define GUM_DEFAULT_CS_ENDIAN CS_MODE_BIG_ENDIAN
#endif
#ifdef G_OS_WIN32
# define GUM_NATIVE_ABI            GUM_ABI_WINDOWS
# define GUM_NATIVE_ABI_IS_WINDOWS 1
# define GUM_NATIVE_ABI_IS_UNIX    0
#else
# define GUM_NATIVE_ABI            GUM_ABI_UNIX
# define GUM_NATIVE_ABI_IS_WINDOWS 0
# define GUM_NATIVE_ABI_IS_UNIX    1
#endif

#ifdef GUM_TARGET_ABI_WINDOWS
# define GUM_TARGET_ABI            GUM_ABI_WINDOWS
# define GUM_TARGET_ABI_IS_WINDOWS 1
# define GUM_TARGET_ABI_IS_UNIX    0
#else
# define GUM_TARGET_ABI            GUM_NATIVE_ABI
# define GUM_TARGET_ABI_IS_WINDOWS GUM_NATIVE_ABI_IS_WINDOWS
# define GUM_TARGET_ABI_IS_UNIX    GUM_NATIVE_ABI_IS_UNIX
#endif

enum _GumOS
{
  GUM_OS_NONE,
  GUM_OS_WINDOWS,
  GUM_OS_MACOS,
  GUM_OS_LINUX,
  GUM_OS_IOS,
  GUM_OS_WATCHOS,
  GUM_OS_TVOS,
  GUM_OS_XROS,
  GUM_OS_ANDROID,
  GUM_OS_FREEBSD,
  GUM_OS_QNX
};

enum _GumCallingConvention
{
  GUM_CALL_CAPI,
  GUM_CALL_SYSAPI
};

enum _GumAbiType
{
  GUM_ABI_UNIX,
  GUM_ABI_WINDOWS
};

typedef enum {
  GUM_CPU_INVALID,
  GUM_CPU_IA32,
  GUM_CPU_AMD64,
  GUM_CPU_ARM,
  GUM_CPU_ARM64,
  GUM_CPU_MIPS
} GumCpuType;

enum _GumCpuFeatures
{
  GUM_CPU_AVX2            = 1 << 0,
  GUM_CPU_AVX512          = 1 << 1,
  GUM_CPU_CET_SS          = 1 << 2,
  GUM_CPU_CET_IBT         = 1 << 3,
  GUM_CPU_THUMB_INTERWORK = 1 << 4,
  GUM_CPU_VFP2            = 1 << 5,
  GUM_CPU_VFP3            = 1 << 6,
  GUM_CPU_VFPD32          = 1 << 7,
  GUM_CPU_PTRAUTH         = 1 << 8,
};

typedef enum {
  GUM_MEMORY_ACCESS_OPEN,
  GUM_MEMORY_ACCESS_EXCLUSIVE,
} GumMemoryAccess;

enum _GumInstructionEncoding
{
  GUM_INSTRUCTION_DEFAULT,
  GUM_INSTRUCTION_SPECIAL
};

enum _GumArgType
{
  GUM_ARG_ADDRESS,
  GUM_ARG_REGISTER
};

struct _GumArgument
{
  GumArgType type;

  union
  {
    GumAddress address;
    gint reg;
  } value;
};

enum _GumBranchHint
{
  GUM_NO_HINT,
  GUM_LIKELY,
  GUM_UNLIKELY
};

union _GumX86VectorReg
{
  guint8 q[16];
  gdouble d[2];
  gfloat s[4];
};

struct _GumIA32CpuContext
{
  guint32 eip;

  guint32 edi;
  guint32 esi;
  guint32 ebp;
  guint32 esp;
  guint32 ebx;
  guint32 edx;
  guint32 ecx;
  guint32 eax;

  GumX86VectorReg * xmm;
};

struct _GumX64CpuContext
{
  guint64 rip;

  guint64 r15;
  guint64 r14;
  guint64 r13;
  guint64 r12;
  guint64 r11;
  guint64 r10;
  guint64 r9;
  guint64 r8;

  guint64 rdi;
  guint64 rsi;
  guint64 rbp;
  guint64 rsp;
  guint64 rbx;
  guint64 rdx;
  guint64 rcx;
  guint64 rax;

  GumX86VectorReg * xmm;
};

union _GumArmVectorReg
{
  guint8 q[16];
  gdouble d[2];
  gfloat s[4];
};

struct _GumArmCpuContext
{
  guint32 pc;
  guint32 sp;
  guint32 cpsr;

  guint32 r8;
  guint32 r9;
  guint32 r10;
  guint32 r11;
  guint32 r12;

  GumArmVectorReg v[16];

  guint32 _padding;

  guint32 r[8];
  guint32 lr;
};

union _GumArm64VectorReg
{
  guint8 q[16];
  gdouble d;
  gfloat s;
  guint16 h;
  guint8 b;
};

struct _GumArm64CpuContext
{
  guint64 pc;
  guint64 sp;
  guint64 nzcv;

  guint64 x[29];
  guint64 fp;
  guint64 lr;

#ifndef G_OS_NONE
  /* Bare-metal threads can be entered with a near-exhausted kernel stack, so
   * there the trampolines stay integer-only and skip the vector registers. */
  GumArm64VectorReg v[32];
#endif
};

struct _GumMipsCpuContext
{
  /*
   * This structure represents the register state pushed onto the stack by the
   * trampoline which allows us to vector from the original minimal assembly
   * hook to architecture agnostic C code inside frida-gum. These registers are
   * natively sized. Even if some have not been expanded to 64-bits from the
   * MIPS32 architecture MIPS can only perform aligned data access and as such
   * pushing zero extended values is simpler than attempting to push minimally
   * sized data types.
   */
  gsize pc;

  gsize gp;
  gsize sp;
  gsize fp;
  gsize ra;

  gsize hi;
  gsize lo;

  gsize at;

  gsize v0;
  gsize v1;

  gsize a0;
  gsize a1;
  gsize a2;
  gsize a3;

  gsize t0;
  gsize t1;
  gsize t2;
  gsize t3;
  gsize t4;
  gsize t5;
  gsize t6;
  gsize t7;
  gsize t8;
  gsize t9;

  gsize s0;
  gsize s1;
  gsize s2;
  gsize s3;
  gsize s4;
  gsize s5;
  gsize s6;
  gsize s7;

  gsize k0;
  gsize k1;
};

enum _GumRelocationScenario
{
  GUM_SCENARIO_OFFLINE,
  GUM_SCENARIO_ONLINE
};

typedef enum {
  GUM_RELOCATION_DEFAULT,
  GUM_RELOCATION_CHECKED,
  GUM_RELOCATION_UNCHECKED,
  GUM_RELOCATION_FORCED,
} GumRelocationPolicy;

#ifndef __arm__
# if GLIB_SIZEOF_VOID_P == 8
#  define GUM_CPU_CONTEXT_XAX(c) ((c)->rax)
#  define GUM_CPU_CONTEXT_XCX(c) ((c)->rcx)
#  define GUM_CPU_CONTEXT_XDX(c) ((c)->rdx)
#  define GUM_CPU_CONTEXT_XBX(c) ((c)->rbx)
#  define GUM_CPU_CONTEXT_XSP(c) ((c)->rsp)
#  define GUM_CPU_CONTEXT_XBP(c) ((c)->rbp)
#  define GUM_CPU_CONTEXT_XSI(c) ((c)->rsi)
#  define GUM_CPU_CONTEXT_XDI(c) ((c)->rdi)
#  define GUM_CPU_CONTEXT_XIP(c) ((c)->rip)
#  define GUM_CPU_CONTEXT_OFFSET_XAX (G_STRUCT_OFFSET (GumCpuContext, rax))
#  define GUM_CPU_CONTEXT_OFFSET_XCX (G_STRUCT_OFFSET (GumCpuContext, rcx))
#  define GUM_CPU_CONTEXT_OFFSET_XDX (G_STRUCT_OFFSET (GumCpuContext, rdx))
#  define GUM_CPU_CONTEXT_OFFSET_XBX (G_STRUCT_OFFSET (GumCpuContext, rbx))
#  define GUM_CPU_CONTEXT_OFFSET_XSP (G_STRUCT_OFFSET (GumCpuContext, rsp))
#  define GUM_CPU_CONTEXT_OFFSET_XBP (G_STRUCT_OFFSET (GumCpuContext, rbp))
#  define GUM_CPU_CONTEXT_OFFSET_XSI (G_STRUCT_OFFSET (GumCpuContext, rsi))
#  define GUM_CPU_CONTEXT_OFFSET_XDI (G_STRUCT_OFFSET (GumCpuContext, rdi))
#  define GUM_CPU_CONTEXT_OFFSET_XIP (G_STRUCT_OFFSET (GumCpuContext, rip))
# else
#  define GUM_CPU_CONTEXT_XAX(c) ((c)->eax)
#  define GUM_CPU_CONTEXT_XCX(c) ((c)->ecx)
#  define GUM_CPU_CONTEXT_XDX(c) ((c)->edx)
#  define GUM_CPU_CONTEXT_XBX(c) ((c)->ebx)
#  define GUM_CPU_CONTEXT_XSP(c) ((c)->esp)
#  define GUM_CPU_CONTEXT_XBP(c) ((c)->ebp)
#  define GUM_CPU_CONTEXT_XSI(c) ((c)->esi)
#  define GUM_CPU_CONTEXT_XDI(c) ((c)->edi)
#  define GUM_CPU_CONTEXT_XIP(c) ((c)->eip)
#  define GUM_CPU_CONTEXT_OFFSET_XAX (G_STRUCT_OFFSET (GumCpuContext, eax))
#  define GUM_CPU_CONTEXT_OFFSET_XCX (G_STRUCT_OFFSET (GumCpuContext, ecx))
#  define GUM_CPU_CONTEXT_OFFSET_XDX (G_STRUCT_OFFSET (GumCpuContext, edx))
#  define GUM_CPU_CONTEXT_OFFSET_XBX (G_STRUCT_OFFSET (GumCpuContext, ebx))
#  define GUM_CPU_CONTEXT_OFFSET_XSP (G_STRUCT_OFFSET (GumCpuContext, esp))
#  define GUM_CPU_CONTEXT_OFFSET_XBP (G_STRUCT_OFFSET (GumCpuContext, ebp))
#  define GUM_CPU_CONTEXT_OFFSET_XSI (G_STRUCT_OFFSET (GumCpuContext, esi))
#  define GUM_CPU_CONTEXT_OFFSET_XDI (G_STRUCT_OFFSET (GumCpuContext, edi))
#  define GUM_CPU_CONTEXT_OFFSET_XIP (G_STRUCT_OFFSET (GumCpuContext, eip))
# endif
#endif

#define GUM_MAX_PATH                 260
#define GUM_MAX_TYPE_NAME             16
#define GUM_MAX_SYMBOL_NAME         2048

#define GUM_MAX_THREADS              768
#define GUM_MAX_CALL_DEPTH            32
#define GUM_MAX_BACKTRACE_DEPTH       16
#define GUM_MAX_WORST_CASE_INFO_SIZE 128

#define GUM_MAX_LISTENERS_PER_FUNCTION 2
#define GUM_MAX_LISTENER_DATA       1024

#define GUM_MAX_THREAD_RANGES 2

#if defined (HAVE_I386)
# if GLIB_SIZEOF_VOID_P == 8
#  define GUM_CPU_MODE CS_MODE_64
#  define GUM_X86_THUNK
# else
#  define GUM_CPU_MODE CS_MODE_32
#  define GUM_X86_THUNK GUM_FASTCALL
# endif
#else
# if G_BYTE_ORDER == G_LITTLE_ENDIAN
#  define GUM_CPU_MODE CS_MODE_LITTLE_ENDIAN
# else
#  define GUM_CPU_MODE CS_MODE_BIG_ENDIAN
# endif
#endif
#if !defined (G_OS_WIN32) && GLIB_SIZEOF_VOID_P == 8
# define GUM_X86_THUNK_REG_ARG0 GUM_X86_XDI
# define GUM_X86_THUNK_REG_ARG1 GUM_X86_XSI
#else
# define GUM_X86_THUNK_REG_ARG0 GUM_X86_XCX
# define GUM_X86_THUNK_REG_ARG1 GUM_X86_XDX
#endif
#define GUM_RED_ZONE_SIZE 128

#if defined (_M_IX86) || defined (__i386__)
# ifdef _MSC_VER
#  define GUM_CDECL __cdecl
#  define GUM_STDCALL __stdcall
#  define GUM_FASTCALL __fastcall
# else
#  define GUM_CDECL __attribute__ ((cdecl))
#  define GUM_STDCALL __attribute__ ((stdcall))
#  define GUM_FASTCALL __attribute__ ((fastcall))
# endif
#else
# define GUM_CDECL
# define GUM_STDCALL
# define GUM_FASTCALL
#endif

#ifdef _MSC_VER
# define GUM_NOINLINE __declspec (noinline)
#else
# define GUM_NOINLINE __attribute__ ((noinline))
#endif

#define GUM_ALIGN_POINTER(t, p, b) \
    ((t) GSIZE_TO_POINTER (((GPOINTER_TO_SIZE (p) + ((gsize) (b - 1))) & \
        ~((gsize) (b - 1)))))
#define GUM_ALIGN_SIZE(s, b) \
    ((((gsize) s) + ((gsize) (b - 1))) & ~((gsize) (b - 1)))

#define GUM_FUNCPTR_TO_POINTER(f) (GSIZE_TO_POINTER (f))
#define GUM_POINTER_TO_FUNCPTR(t, p) ((t) GPOINTER_TO_SIZE (p))

#define GUM_INT2_MASK  0x00000003U
#define GUM_INT3_MASK  0x00000007U
#define GUM_INT4_MASK  0x0000000fU
#define GUM_INT5_MASK  0x0000001fU
#define GUM_INT6_MASK  0x0000003fU
#define GUM_INT8_MASK  0x000000ffU
#define GUM_INT10_MASK 0x000003ffU
#define GUM_INT11_MASK 0x000007ffU
#define GUM_INT12_MASK 0x00000fffU
#define GUM_INT14_MASK 0x00003fffU
#define GUM_INT16_MASK 0x0000ffffU
#define GUM_INT18_MASK 0x0003ffffU
#define GUM_INT19_MASK 0x0007ffffU
#define GUM_INT24_MASK 0x00ffffffU
#define GUM_INT26_MASK 0x03ffffffU
#define GUM_INT28_MASK 0x0fffffffU
#define GUM_INT32_MASK 0xffffffffU

#define GUM_IS_WITHIN_UINT7_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (0) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (127))
#define GUM_IS_WITHIN_UINT8_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (0) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (255))
#define GUM_IS_WITHIN_INT8_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-128) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (127))
#define GUM_IS_WITHIN_INT11_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-1024) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (1023))
#define GUM_IS_WITHIN_INT14_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-8192) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (8191))
#define GUM_IS_WITHIN_INT16_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-32768) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (32767))
#define GUM_IS_WITHIN_INT18_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-131072) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (131071))
#define GUM_IS_WITHIN_INT19_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-262144) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (262143))
#define GUM_IS_WITHIN_INT20_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-524288) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (524287))
#define GUM_IS_WITHIN_INT21_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-1048576) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (1048575))
#define GUM_IS_WITHIN_INT24_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-8388608) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (8388607))
#define GUM_IS_WITHIN_INT26_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-33554432) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (33554431))
#define GUM_IS_WITHIN_INT28_RANGE(i) \
    (((gint64) (i)) >= G_GINT64_CONSTANT (-134217728) && \
     ((gint64) (i)) <= G_GINT64_CONSTANT (134217727))
#define GUM_IS_WITHIN_INT32_RANGE(i) \
    (((gint64) (i)) >= (gint64) G_MININT32 && \
     ((gint64) (i)) <= (gint64) G_MAXINT32)

#ifdef G_NORETURN
# define GUM_NORETURN G_NORETURN
#else
# define GUM_NORETURN
#endif

GUM_API GQuark gum_error_quark (void);

GUM_API GUM_NORETURN void gum_panic (const gchar * format, ...)
    G_ANALYZER_NORETURN;

GUM_API GumCpuFeatures gum_query_cpu_features (void);

GUM_API gpointer gum_cpu_context_get_nth_argument (GumCpuContext * self,
    guint n);
GUM_API void gum_cpu_context_replace_nth_argument (GumCpuContext * self,
    guint n, gpointer value);
GUM_API gpointer gum_cpu_context_get_return_value (GumCpuContext * self);
GUM_API void gum_cpu_context_replace_return_value (GumCpuContext * self,
    gpointer value);

#define GUM_TYPE_CPU_CONTEXT (gum_cpu_context_get_type ())
GUM_API GType gum_cpu_context_get_type (void) G_GNUC_CONST;
GUM_API GumCpuContext * gum_cpu_context_copy (
    const GumCpuContext * cpu_context);
GUM_API void gum_cpu_context_free (GumCpuContext * cpu_context);

GUM_API GType gum_address_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif
