/*
 * Copyright (C) 2024-2025 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __GUM_UNWIND_BROKER_H__
#define __GUM_UNWIND_BROKER_H__

#include <gum/gumdefs.h>
#include <gum/gummemory.h>

G_BEGIN_DECLS

#define GUM_TYPE_UNWIND_BROKER (gum_unwind_broker_get_type ())
G_DECLARE_FINAL_TYPE (GumUnwindBroker, gum_unwind_broker, GUM, UNWIND_BROKER,
                      GObject)

#define GUM_TYPE_UNWIND_SECTIONS_PROVIDER \
    (gum_unwind_sections_provider_get_type ())
G_DECLARE_INTERFACE (GumUnwindSectionsProvider, gum_unwind_sections_provider,
                     GUM, UNWIND_SECTIONS_PROVIDER, GObject)

#define GUM_TYPE_UNWIND_PC_TRANSLATOR (gum_unwind_pc_translator_get_type ())
G_DECLARE_INTERFACE (GumUnwindPcTranslator, gum_unwind_pc_translator, GUM,
                     UNWIND_PC_TRANSLATOR, GObject)

struct _GumUnwindSectionsProviderInterface
{
  GTypeInterface parent;

  const GumMemoryRange * (* get_range) (GumUnwindSectionsProvider * self);
  gboolean (* fill) (GumUnwindSectionsProvider * self, GumAddress address,
      gpointer info);
};

struct _GumUnwindPcTranslatorInterface
{
  GTypeInterface parent;

  GumAddress (* translate) (GumUnwindPcTranslator * self,
      GumAddress code_address);
  gboolean (* install_resume_context) (GumUnwindPcTranslator * self,
      gpointer unwind_context, GumAddress real_resume_ip);
};

GUM_API GumUnwindBroker * gum_unwind_broker_obtain (void);

GUM_API void gum_unwind_broker_add_sections_provider (GumUnwindBroker * self,
    GumUnwindSectionsProvider * provider);
GUM_API void gum_unwind_broker_remove_sections_provider (GumUnwindBroker * self,
    GumUnwindSectionsProvider * provider);

GUM_API void gum_unwind_broker_add_pc_translator (GumUnwindBroker * self,
    GumUnwindPcTranslator * translator);
GUM_API void gum_unwind_broker_remove_pc_translator (GumUnwindBroker * self,
    GumUnwindPcTranslator * translator);

GUM_API const GumMemoryRange * gum_unwind_sections_provider_get_range (
    GumUnwindSectionsProvider * self);
GUM_API gboolean gum_unwind_sections_provider_fill (
    GumUnwindSectionsProvider * self, GumAddress address, gpointer info);

GUM_API GumAddress gum_unwind_pc_translator_translate (
    GumUnwindPcTranslator * self, GumAddress code_address);
GUM_API gboolean gum_unwind_pc_translator_install_resume_context (
    GumUnwindPcTranslator * self, gpointer unwind_context,
    GumAddress real_resume_ip);

G_END_DECLS

#endif
