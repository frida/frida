/*
 * Copyright (C) 2015-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2020 Francesco Tamagni <mrmacete@protonmail.ch>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_EXCEPTOR_H__
#define __GUM_EXCEPTOR_H__

#include <gum/gummemory.h>
#include <gum/gumprocess.h>
#include <setjmp.h>

G_BEGIN_DECLS

#define GUM_TYPE_EXCEPTOR (gum_exceptor_get_type ())
G_DECLARE_FINAL_TYPE (GumExceptor, gum_exceptor, GUM, EXCEPTOR, GObject)

typedef enum {
  GUM_EXCEPTOR_MODE_FULL,
  GUM_EXCEPTOR_MODE_HANDLER_ONLY,
  GUM_EXCEPTOR_MODE_OFF
} GumExceptorMode;

#if defined (GUM_GIR_COMPILATION)
  typedef int GumExceptorNativeJmpBuf;
#elif defined (G_OS_WIN32) || defined (G_OS_NONE) || defined (__APPLE__)
# define GUM_NATIVE_SETJMP(env) setjmp (env)
# define GUM_NATIVE_LONGJMP longjmp
  typedef jmp_buf GumExceptorNativeJmpBuf;
#else
# define GUM_NATIVE_SETJMP(env) sigsetjmp (env, TRUE)
# define GUM_NATIVE_LONGJMP siglongjmp
  typedef sigjmp_buf GumExceptorNativeJmpBuf;
#endif

typedef struct _GumExceptionDetails GumExceptionDetails;
typedef guint GumExceptionType;
typedef struct _GumExceptionMemoryDetails GumExceptionMemoryDetails;
typedef gboolean (* GumExceptionHandler) (GumExceptionDetails * details,
    gpointer user_data);

typedef struct _GumExceptorScope GumExceptorScope;

enum _GumExceptionType
{
  GUM_EXCEPTION_ABORT = 1,
  GUM_EXCEPTION_ACCESS_VIOLATION,
  GUM_EXCEPTION_GUARD_PAGE,
  GUM_EXCEPTION_ILLEGAL_INSTRUCTION,
  GUM_EXCEPTION_STACK_OVERFLOW,
  GUM_EXCEPTION_ARITHMETIC,
  GUM_EXCEPTION_BREAKPOINT,
  GUM_EXCEPTION_SINGLE_STEP,
  GUM_EXCEPTION_SYSTEM
};

struct _GumExceptionMemoryDetails
{
  GumMemoryOperation operation;
  gpointer address;
};

struct _GumExceptionDetails
{
  GumThreadId thread_id;
  GumExceptionType type;
  gpointer address;
  GumExceptionMemoryDetails memory;
  GumCpuContext context;
  gpointer native_context;
};

struct _GumExceptorScope
{
  GumExceptionDetails exception;

  /*< private */
  gboolean exception_occurred;
  gpointer padding[2];
  GumExceptorNativeJmpBuf env;
#ifdef __ANDROID__
  sigset_t mask;
#endif

  GumExceptorScope * next;
};

GUM_API void gum_exceptor_set_mode (GumExceptorMode mode);

GUM_API GumExceptor * gum_exceptor_obtain (void);

GUM_API void gum_exceptor_reset (GumExceptor * self);

GUM_API void gum_exceptor_add (GumExceptor * self, GumExceptionHandler func,
    gpointer user_data);
GUM_API void gum_exceptor_remove (GumExceptor * self, GumExceptionHandler func,
    gpointer user_data);

#if defined (_MSC_VER) && defined (HAVE_I386) && GLIB_SIZEOF_VOID_P == 8
/*
 * On MSVC/x86_64 setjmp() is actually an intrinsic that calls _setjmp() with a
 * a hidden second argument specifying the frame pointer. This makes sense when
 * the longjmp() is guaranteed to happen from code we control, but is not
 * reliable otherwise.
 */
# define gum_exceptor_try(self, scope) ( \
    _gum_exceptor_prepare_try (self, scope), \
    ((int (*) (jmp_buf env, void * frame_pointer)) _setjmp) ( \
        (scope)->env, NULL) == 0)
#else
# define gum_exceptor_try(self, scope) ( \
    _gum_exceptor_prepare_try (self, scope), \
    GUM_NATIVE_SETJMP ((scope)->env) == 0)
#endif
GUM_API gboolean gum_exceptor_catch (GumExceptor * self,
    GumExceptorScope * scope);
GUM_API gboolean gum_exceptor_has_scope (GumExceptor * self,
    GumThreadId thread_id);

GUM_API gchar * gum_exception_details_to_string (
    const GumExceptionDetails * details);

GUM_API void _gum_exceptor_prepare_try (GumExceptor * self,
    GumExceptorScope * scope);

G_END_DECLS

#endif
