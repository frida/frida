/*
 * Copyright (C) 2024-2025 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumunwindbroker-priv.h"

#include "guminterceptor.h"

static GumAddress gum_unwind_broker_translate_pc (GumAddress code_address);

static GumInterceptor * gum_unwind_libunwind_interceptor = NULL;

#ifdef HAVE_ARM

/*
 * On 32-bit ARM the C++ runtime uses the EH ABI: the personality routine
 * takes a different set of arguments, FDE lookup goes through
 * __gnu_Unwind_Find_exidx() instead of _Unwind_Find_FDE(), and the
 * instruction pointer is accessed through _Unwind_VRS_Get()/_Unwind_VRS_Set().
 * We declare the bits we need directly so the file does not depend on whichever
 * <unwind.h> happens to be on the include path.
 */

typedef int _Unwind_Reason_Code;
typedef unsigned _Unwind_State;
typedef gsize _Unwind_Ptr;
struct _Unwind_Control_Block;
struct _Unwind_Context;

#define GUM_URC_INSTALL_CONTEXT 7
#define GUM_UVRSC_CORE 0
#define GUM_UVRSD_UINT32 0
#define GUM_UNWIND_ARM_REG_PC 15

extern _Unwind_Reason_Code __gxx_personality_v0 (_Unwind_State state,
    struct _Unwind_Control_Block * ucbp, struct _Unwind_Context * context)
    __attribute__ ((weak));
extern _Unwind_Ptr __gnu_Unwind_Find_exidx (_Unwind_Ptr pc, int * nrec);
extern int _Unwind_VRS_Get (struct _Unwind_Context * context, int regclass,
    guint32 regno, int representation, void * valuep);
extern int _Unwind_VRS_Set (struct _Unwind_Context * context, int regclass,
    guint32 regno, int representation, void * valuep);

static _Unwind_Reason_Code gum_unwind_broker_replacement_personality (
    _Unwind_State state, struct _Unwind_Control_Block * ucbp,
    struct _Unwind_Context * context);
static _Unwind_Ptr gum_unwind_broker_replacement_find_exidx (_Unwind_Ptr pc,
    int * nrec);
static GumAddress gum_unwind_broker_get_ip (struct _Unwind_Context * context);

void
_gum_unwind_broker_backend_activate (void)
{
  GumReplaceReturn res G_GNUC_UNUSED;

  if (__gxx_personality_v0 == NULL)
    return;

  gum_unwind_libunwind_interceptor = gum_interceptor_obtain ();

  res = gum_interceptor_replace (gum_unwind_libunwind_interceptor,
      __gxx_personality_v0, gum_unwind_broker_replacement_personality, NULL,
      NULL);
  g_assert (res == GUM_REPLACE_OK);

  res = gum_interceptor_replace (gum_unwind_libunwind_interceptor,
      __gnu_Unwind_Find_exidx, gum_unwind_broker_replacement_find_exidx, NULL,
      NULL);
  g_assert (res == GUM_REPLACE_OK);
}

void
_gum_unwind_broker_backend_deactivate (void)
{
  if (gum_unwind_libunwind_interceptor == NULL)
    return;

  gum_interceptor_revert (gum_unwind_libunwind_interceptor,
      __gxx_personality_v0);
  gum_interceptor_revert (gum_unwind_libunwind_interceptor,
      __gnu_Unwind_Find_exidx);

  g_object_unref (gum_unwind_libunwind_interceptor);
  gum_unwind_libunwind_interceptor = NULL;
}

static _Unwind_Reason_Code
gum_unwind_broker_replacement_personality (_Unwind_State state,
                                           struct _Unwind_Control_Block * ucbp,
                                           struct _Unwind_Context * context)
{
  _Unwind_Reason_Code reason;
  GumAddress throw_ip, real_throw_ip;

  throw_ip = gum_unwind_broker_get_ip (context);
  real_throw_ip = gum_unwind_broker_translate_pc (throw_ip);
  if (real_throw_ip == 0)
    return __gxx_personality_v0 (state, ucbp, context);

  _gum_unwind_broker_set_ip (context, real_throw_ip);

  reason = __gxx_personality_v0 (state, ucbp, context);
  if (reason == GUM_URC_INSTALL_CONTEXT)
  {
    GumAddress real_resume_ip = gum_unwind_broker_get_ip (context);

    _gum_unwind_broker_dispatch_install_resume_context (context,
        real_resume_ip);
  }

  return reason;
}

static _Unwind_Ptr
gum_unwind_broker_replacement_find_exidx (_Unwind_Ptr pc,
                                          int * nrec)
{
  GumAddress real_address;

  real_address = gum_unwind_broker_translate_pc (pc + 1);
  if (real_address == 0)
    return __gnu_Unwind_Find_exidx (pc, nrec);

  return __gnu_Unwind_Find_exidx ((_Unwind_Ptr) (real_address - 1), nrec);
}

static GumAddress
gum_unwind_broker_get_ip (struct _Unwind_Context * context)
{
  guint32 value = 0;

  _Unwind_VRS_Get (context, GUM_UVRSC_CORE, GUM_UNWIND_ARM_REG_PC,
      GUM_UVRSD_UINT32, &value);

  return value & ~(GumAddress) 1;
}

void
_gum_unwind_broker_set_ip (gpointer unwind_context,
                           GumAddress ip)
{
  struct _Unwind_Context * context = unwind_context;
  guint32 thumb_bit = 0;
  guint32 value;

  _Unwind_VRS_Get (context, GUM_UVRSC_CORE, GUM_UNWIND_ARM_REG_PC,
      GUM_UVRSD_UINT32, &thumb_bit);
  thumb_bit &= 1;

  value = (guint32) ip | thumb_bit;
  _Unwind_VRS_Set (context, GUM_UVRSC_CORE, GUM_UNWIND_ARM_REG_PC,
      GUM_UVRSD_UINT32, &value);
}

#else

#include <unwind.h>

typedef struct _Unwind_Exception _Unwind_Exception;
typedef struct _Unwind_Context _Unwind_Context;
struct dwarf_eh_bases;

typedef unsigned long (* GumUnwindGetIpFunc) (struct _Unwind_Context *);

extern _Unwind_Reason_Code __gxx_personality_v0 (int version,
    _Unwind_Action actions, uint64_t exception_class,
    _Unwind_Exception * unwind_exception, _Unwind_Context * context)
    __attribute__ ((weak));
extern const void * _Unwind_Find_FDE (const void * pc,
    struct dwarf_eh_bases *);
extern unsigned long _Unwind_GetIP (struct _Unwind_Context *);

static _Unwind_Reason_Code gum_unwind_broker_replacement_personality (
    int version, _Unwind_Action actions, uint64_t exception_class,
    _Unwind_Exception * unwind_exception, _Unwind_Context * context);
static GumAddress gum_unwind_get_untranslated_ip (
    struct _Unwind_Context * context);
static const void * gum_unwind_broker_replacement_find_fde (const void * pc,
    struct dwarf_eh_bases * bases);
static unsigned long gum_unwind_broker_replacement_get_ip (
    struct _Unwind_Context * context);

static gpointer gum_unwind_real_get_ip;

void
_gum_unwind_broker_backend_activate (void)
{
  GumReplaceReturn res G_GNUC_UNUSED;

  if (__gxx_personality_v0 == NULL)
    return;

  gum_unwind_libunwind_interceptor = gum_interceptor_obtain ();

  res = gum_interceptor_replace (gum_unwind_libunwind_interceptor,
      __gxx_personality_v0, gum_unwind_broker_replacement_personality, NULL,
      NULL);
  g_assert (res == GUM_REPLACE_OK);

  res = gum_interceptor_replace (gum_unwind_libunwind_interceptor,
      _Unwind_Find_FDE, gum_unwind_broker_replacement_find_fde, NULL, NULL);
  g_assert (res == GUM_REPLACE_OK);

  res = gum_interceptor_replace (gum_unwind_libunwind_interceptor,
      _Unwind_GetIP, gum_unwind_broker_replacement_get_ip,
      &gum_unwind_real_get_ip, NULL);
  g_assert (res == GUM_REPLACE_OK);
}

void
_gum_unwind_broker_backend_deactivate (void)
{
  if (gum_unwind_libunwind_interceptor == NULL)
    return;

  gum_interceptor_revert (gum_unwind_libunwind_interceptor,
      __gxx_personality_v0);
  gum_interceptor_revert (gum_unwind_libunwind_interceptor, _Unwind_Find_FDE);
  gum_interceptor_revert (gum_unwind_libunwind_interceptor, _Unwind_GetIP);

  g_object_unref (gum_unwind_libunwind_interceptor);
  gum_unwind_libunwind_interceptor = NULL;
}

static _Unwind_Reason_Code
gum_unwind_broker_replacement_personality (int version,
                                           _Unwind_Action actions,
                                           uint64_t exception_class,
                                           _Unwind_Exception * unwind_exception,
                                           _Unwind_Context * context)
{
  _Unwind_Reason_Code reason;
  GumAddress throw_ip, real_throw_ip;

  throw_ip = gum_unwind_get_untranslated_ip (context);
  real_throw_ip = gum_unwind_broker_translate_pc (throw_ip);
  if (real_throw_ip == 0)
  {
    return __gxx_personality_v0 (version, actions, exception_class,
        unwind_exception, context);
  }

  _Unwind_SetIP (context, real_throw_ip);

  reason = __gxx_personality_v0 (version, actions, exception_class,
      unwind_exception, context);
  if (reason == _URC_INSTALL_CONTEXT)
  {
    GumAddress real_resume_ip = gum_unwind_get_untranslated_ip (context);

    _gum_unwind_broker_dispatch_install_resume_context (context,
        real_resume_ip);
  }

  return reason;
}

static GumAddress
gum_unwind_get_untranslated_ip (struct _Unwind_Context * context)
{
  GumUnwindGetIpFunc get_ip =
      GUM_POINTER_TO_FUNCPTR (GumUnwindGetIpFunc, gum_unwind_real_get_ip);

  return get_ip (context);
}

static const void *
gum_unwind_broker_replacement_find_fde (const void * pc,
                                        struct dwarf_eh_bases * bases)
{
  GumAddress real_address;

  real_address = gum_unwind_broker_translate_pc (GUM_ADDRESS (pc) + 1);
  if (real_address == 0)
    return _Unwind_Find_FDE (pc, bases);

  return _Unwind_Find_FDE (GSIZE_TO_POINTER (real_address - 1), bases);
}

static unsigned long
gum_unwind_broker_replacement_get_ip (struct _Unwind_Context * context)
{
  GumAddress ip, real_address;

  ip = _Unwind_GetIP (context);
  real_address = gum_unwind_broker_translate_pc (ip);
  if (real_address == 0)
    return ip;

  return real_address;
}

void
_gum_unwind_broker_set_ip (gpointer unwind_context,
                           GumAddress ip)
{
  _Unwind_SetIP ((struct _Unwind_Context *) unwind_context, ip);
}

#endif

static GumAddress
gum_unwind_broker_translate_pc (GumAddress code_address)
{
  gpointer translated;
  GumAddress result;

  translated = gum_invocation_stack_translate (
      gum_interceptor_get_current_stack (), GSIZE_TO_POINTER (code_address));
  if (translated != GSIZE_TO_POINTER (code_address))
    return GUM_ADDRESS (translated);

  result = _gum_unwind_broker_dispatch_translate (code_address);
  if (result == code_address)
    return 0;

  return result;
}
