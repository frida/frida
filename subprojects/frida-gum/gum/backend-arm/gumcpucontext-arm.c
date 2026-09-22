/*
 * Copyright (C) 2010-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
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

gpointer
gum_cpu_context_get_nth_argument (GumCpuContext * self,
                                  guint n)
{
  if (n < 4)
  {
    return (gpointer) self->r[n];
  }
  else
  {
    gpointer * stack_argument = (gpointer *) self->sp;

    return stack_argument[n - 4];
  }
}

void
gum_cpu_context_replace_nth_argument (GumCpuContext * self,
                                      guint n,
                                      gpointer value)
{
  if (n < 4)
  {
    self->r[n] = (guint32) value;
  }
  else
  {
    gpointer * stack_argument = (gpointer *) self->sp;

    stack_argument[n - 4] = value;
  }
}

gpointer
gum_cpu_context_get_return_value (GumCpuContext * self)
{
  return (gpointer) self->r[0];
}

void
gum_cpu_context_replace_return_value (GumCpuContext * self,
                                      gpointer value)
{
  self->r[0] = (guint32) value;
}
