#ifndef __GUMDEFS_H__
#define __GUMDEFS_H__

#include <glib.h>

#define GUM_ADDRESS(a) ((GumAddress) (guintptr) (a))

typedef guint64 GumAddress;
typedef guint GumOS;
typedef guint GumCallingConvention;
typedef guint GumAbiType;
typedef guint GumCpuType;
typedef guint GumCpuFeatures;
typedef guint GumInstructionEncoding;
typedef guint GumArgType;
typedef struct _GumArgument GumArgument;
typedef guint GumBranchHint;
typedef struct _GumCpuContext GumCpuContext;
typedef union _GumX86VectorReg GumX86VectorReg;
typedef union _GumArmVectorReg GumArmVectorReg;
typedef union _GumArm64VectorReg GumArm64VectorReg;
typedef struct _GumMemoryRange GumMemoryRange;

enum _GumCallingConvention
{
  GUM_CALL_CAPI,
  GUM_CALL_SYSAPI
};

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

union _GumArmVectorReg
{
  guint8 q[16];
  gdouble d[2];
  gfloat s[4];
};

union _GumArm64VectorReg
{
  guint8 q[16];
  gdouble d;
  gfloat s;
  guint16 h;
  guint8 b;
};

struct _GumCpuContext
{
#if defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 4
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
#elif defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
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
#elif defined (HAVE_ARM)
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
#elif defined (HAVE_ARM64)
  guint64 pc;
  guint64 sp;
  guint64 nzcv;

  guint64 x[29];
  guint64 fp;
  guint64 lr;

  GumArm64VectorReg v[32];
#elif defined (HAVE_MIPS)
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
#endif
};

struct _GumMemoryRange
{
  GumAddress base_address;
  gsize size;
};

#endif
