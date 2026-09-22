/*
 * Copyright (C) 2014-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C)      2019 Jon Wilson <jonwilson@zepler.net>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumdefs.h"

G_DEFINE_BOXED_TYPE (GumCpuContext, gum_cpu_context, gum_cpu_context_copy,
    gum_cpu_context_free)

GumCpuContext *
gum_cpu_context_copy (const GumCpuContext * cpu_context)
{
  return g_memdup2 (cpu_context, sizeof (GumCpuContext));
}

void
gum_cpu_context_free (GumCpuContext * cpu_context)
{
  g_free (cpu_context);
}

#if GLIB_SIZEOF_VOID_P == 4

gpointer
gum_cpu_context_get_nth_argument (GumCpuContext * self,
                                  guint n)
{
  if (n < 4)
  {
    switch (n)
    {
      case 0:
        return (gpointer) self->a0;
      case 1:
        return (gpointer) self->a1;
      case 2:
        return (gpointer) self->a2;
      case 3:
        return (gpointer) self->a3;
    }
  }
  else
  {
    gpointer * stack_argument = (gpointer *) (self->sp + 0x14);

    return stack_argument[n - 4];
  }

  return NULL;
}

void
gum_cpu_context_replace_nth_argument (GumCpuContext * self,
                                      guint n,
                                      gpointer value)
{
  if (n < 4)
  {
    switch (n)
    {
      case 0:
        self->a0 = (guint32) value;
        break;
      case 1:
        self->a1 = (guint32) value;
        break;
      case 2:
        self->a2 = (guint32) value;
        break;
      case 3:
        self->a3 = (guint32) value;
        break;
    }
  }
  else
  {
    gpointer * stack_argument = (gpointer *) (self->sp + 0x14);

    stack_argument[n - 4] = value;
  }
}

#else

/*
 * On MIPS64, 8 registers are used for passing arguments, t0 through t3 are
 * renamed a4-a7 in the instruction set architecture.
 */

gpointer
gum_cpu_context_get_nth_argument (GumCpuContext * self,
                                  guint n)
{
  if (n < 8)
  {
    switch (n)
    {
      case 0:
        return (gpointer) self->a0;
      case 1:
        return (gpointer) self->a1;
      case 2:
        return (gpointer) self->a2;
      case 3:
        return (gpointer) self->a3;
      case 4:
        return (gpointer) self->t0;
      case 5:
        return (gpointer) self->t1;
      case 6:
        return (gpointer) self->t2;
      case 7:
        return (gpointer) self->t3;
    }
  }
  else
  {
    gpointer * stack_argument = (gpointer *) (self->sp + 0x8);

    return stack_argument[n - 8];
  }

  return NULL;
}

void
gum_cpu_context_replace_nth_argument (GumCpuContext * self,
                                      guint n,
                                      gpointer value)
{
  if (n < 8)
  {
    switch (n)
    {
      case 0:
        self->a0 = (guint64) value;
        break;
      case 1:
        self->a1 = (guint64) value;
        break;
      case 2:
        self->a2 = (guint64) value;
        break;
      case 3:
        self->a3 = (guint64) value;
        break;
      case 4:
        self->t0 = (guint64) value;
        break;
      case 5:
        self->t1 = (guint64) value;
        break;
      case 6:
        self->t2 = (guint64) value;
        break;
      case 7:
        self->t3 = (guint64) value;
        break;
    }
  }
  else
  {
    gpointer * stack_argument = (gpointer *) (self->sp + 0x8);

    stack_argument[n - 8] = value;
  }
}

#endif

gpointer
gum_cpu_context_get_return_value (GumCpuContext * self)
{
  return (gpointer) self->v0;
}

void
gum_cpu_context_replace_return_value (GumCpuContext * self,
                                      gpointer value)
{
  self->v0 = GPOINTER_TO_SIZE (value);
}
