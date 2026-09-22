/*
 * Copyright (C) 2024-2025 Francesco Tamagni <mrmacete@protonmail.ch>
 * Copyright (C) 2024-2026 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "gumunwindbroker.h"

#include "gumunwindbroker-priv.h"

struct _GumUnwindBroker
{
  GObject parent;

  GMutex mutex;

  GPtrArray * sections_providers;
  GPtrArray * pc_translators;
};

G_DEFINE_TYPE (GumUnwindBroker, gum_unwind_broker, G_TYPE_OBJECT)
G_DEFINE_INTERFACE (GumUnwindSectionsProvider, gum_unwind_sections_provider,
                    G_TYPE_OBJECT)
G_DEFINE_INTERFACE (GumUnwindPcTranslator, gum_unwind_pc_translator,
                    G_TYPE_OBJECT)

static void gum_unwind_broker_dispose (GObject * object);
static void gum_unwind_broker_finalize (GObject * object);

static void the_unwind_broker_weak_notify (gpointer data,
    GObject * where_the_object_was);

static gboolean gum_unwind_pc_translator_default_install_resume_context (
    GumUnwindPcTranslator * self, gpointer unwind_context,
    GumAddress real_resume_ip);

G_LOCK_DEFINE_STATIC (the_unwind_broker);
static GumUnwindBroker * the_unwind_broker = NULL;

static void
gum_unwind_sections_provider_default_init (
    GumUnwindSectionsProviderInterface * iface)
{
}

static void
gum_unwind_pc_translator_default_init (
    GumUnwindPcTranslatorInterface * iface)
{
  iface->install_resume_context =
      gum_unwind_pc_translator_default_install_resume_context;
}

static void
gum_unwind_broker_class_init (GumUnwindBrokerClass * klass)
{
  GObjectClass * object_class = G_OBJECT_CLASS (klass);

  object_class->dispose = gum_unwind_broker_dispose;
  object_class->finalize = gum_unwind_broker_finalize;
}

static void
gum_unwind_broker_init (GumUnwindBroker * self)
{
  g_mutex_init (&self->mutex);

  self->sections_providers = g_ptr_array_new ();
  self->pc_translators = g_ptr_array_new ();
}

static void
gum_unwind_broker_dispose (GObject * object)
{
  GumUnwindBroker * self = GUM_UNWIND_BROKER (object);

  g_mutex_lock (&self->mutex);
  g_ptr_array_set_size (self->sections_providers, 0);
  g_ptr_array_set_size (self->pc_translators, 0);
  g_mutex_unlock (&self->mutex);

  _gum_unwind_broker_backend_deactivate ();

  G_OBJECT_CLASS (gum_unwind_broker_parent_class)->dispose (object);
}

static void
gum_unwind_broker_finalize (GObject * object)
{
  GumUnwindBroker * self = GUM_UNWIND_BROKER (object);

  g_ptr_array_unref (self->sections_providers);
  g_ptr_array_unref (self->pc_translators);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (gum_unwind_broker_parent_class)->finalize (object);
}

GumUnwindBroker *
gum_unwind_broker_obtain (void)
{
  GumUnwindBroker * broker;
  gboolean newly_created = FALSE;

  G_LOCK (the_unwind_broker);

  if (the_unwind_broker != NULL)
  {
    broker = g_object_ref (the_unwind_broker);
  }
  else
  {
    the_unwind_broker = g_object_new (GUM_TYPE_UNWIND_BROKER, NULL);
    g_object_weak_ref (G_OBJECT (the_unwind_broker),
        the_unwind_broker_weak_notify, NULL);

    broker = the_unwind_broker;
    newly_created = TRUE;
  }

  G_UNLOCK (the_unwind_broker);

  if (newly_created)
    _gum_unwind_broker_backend_activate ();

  return broker;
}

static void
the_unwind_broker_weak_notify (gpointer data,
                               GObject * where_the_object_was)
{
  G_LOCK (the_unwind_broker);

  the_unwind_broker = NULL;

  G_UNLOCK (the_unwind_broker);
}

void
gum_unwind_broker_add_sections_provider (GumUnwindBroker * self,
                                         GumUnwindSectionsProvider * provider)
{
  g_mutex_lock (&self->mutex);
  g_ptr_array_add (self->sections_providers, provider);
  g_mutex_unlock (&self->mutex);
}

void
gum_unwind_broker_remove_sections_provider (
    GumUnwindBroker * self,
    GumUnwindSectionsProvider * provider)
{
  g_mutex_lock (&self->mutex);
  g_ptr_array_remove (self->sections_providers, provider);
  g_mutex_unlock (&self->mutex);
}

void
gum_unwind_broker_add_pc_translator (GumUnwindBroker * self,
                                     GumUnwindPcTranslator * translator)
{
  g_mutex_lock (&self->mutex);
  g_ptr_array_add (self->pc_translators, translator);
  g_mutex_unlock (&self->mutex);
}

void
gum_unwind_broker_remove_pc_translator (GumUnwindBroker * self,
                                        GumUnwindPcTranslator * translator)
{
  g_mutex_lock (&self->mutex);
  g_ptr_array_remove (self->pc_translators, translator);
  g_mutex_unlock (&self->mutex);
}

gboolean
_gum_unwind_broker_dispatch_sections (GumAddress address,
                                      gpointer info)
{
  gboolean handled = FALSE;
  GumUnwindBroker * self;
  guint i;

  G_LOCK (the_unwind_broker);
  self = (the_unwind_broker != NULL) ? g_object_ref (the_unwind_broker) : NULL;
  G_UNLOCK (the_unwind_broker);

  if (self == NULL)
    return FALSE;

  g_mutex_lock (&self->mutex);
  for (i = 0; i != self->sections_providers->len; i++)
  {
    GumUnwindSectionsProvider * provider =
        g_ptr_array_index (self->sections_providers, i);
    const GumMemoryRange * range =
        gum_unwind_sections_provider_get_range (provider);
    GumAddress end = range->base_address + range->size;

    if (address >= range->base_address && address < end)
    {
      handled = gum_unwind_sections_provider_fill (provider, address, info);
      if (handled)
        break;
    }
  }
  g_mutex_unlock (&self->mutex);

  g_object_unref (self);

  return handled;
}

GumAddress
_gum_unwind_broker_dispatch_translate (GumAddress code_address)
{
  GumAddress result = 0;
  GumUnwindBroker * self;
  guint i;

  G_LOCK (the_unwind_broker);
  self = (the_unwind_broker != NULL) ? g_object_ref (the_unwind_broker) : NULL;
  G_UNLOCK (the_unwind_broker);

  if (self == NULL)
    return 0;

  g_mutex_lock (&self->mutex);
  for (i = 0; i != self->pc_translators->len; i++)
  {
    GumUnwindPcTranslator * translator =
        g_ptr_array_index (self->pc_translators, i);

    result = gum_unwind_pc_translator_translate (translator, code_address);
    if (result != 0)
      break;
  }
  g_mutex_unlock (&self->mutex);

  g_object_unref (self);

  return result;
}

gboolean
_gum_unwind_broker_dispatch_install_resume_context (gpointer unwind_context,
                                                    GumAddress real_resume_ip)
{
  gboolean handled = FALSE;
  GumUnwindBroker * self;
  guint i;

  G_LOCK (the_unwind_broker);
  self = (the_unwind_broker != NULL) ? g_object_ref (the_unwind_broker) : NULL;
  G_UNLOCK (the_unwind_broker);

  if (self == NULL)
    return FALSE;

  g_mutex_lock (&self->mutex);
  for (i = 0; i != self->pc_translators->len; i++)
  {
    GumUnwindPcTranslator * translator =
        g_ptr_array_index (self->pc_translators, i);

    handled = gum_unwind_pc_translator_install_resume_context (translator,
        unwind_context, real_resume_ip);
    if (handled)
      break;
  }
  g_mutex_unlock (&self->mutex);

  g_object_unref (self);

  return handled;
}

const GumMemoryRange *
gum_unwind_sections_provider_get_range (GumUnwindSectionsProvider * self)
{
  return GUM_UNWIND_SECTIONS_PROVIDER_GET_IFACE (self)->get_range (self);
}

gboolean
gum_unwind_sections_provider_fill (GumUnwindSectionsProvider * self,
                                   GumAddress address,
                                   gpointer info)
{
  return GUM_UNWIND_SECTIONS_PROVIDER_GET_IFACE (self)->fill (self, address,
      info);
}

GumAddress
gum_unwind_pc_translator_translate (GumUnwindPcTranslator * self,
                                    GumAddress code_address)
{
  return GUM_UNWIND_PC_TRANSLATOR_GET_IFACE (self)->translate (self,
      code_address);
}

gboolean
gum_unwind_pc_translator_install_resume_context (GumUnwindPcTranslator * self,
                                                 gpointer unwind_context,
                                                 GumAddress real_resume_ip)
{
  return GUM_UNWIND_PC_TRANSLATOR_GET_IFACE (self)->install_resume_context (
      self, unwind_context, real_resume_ip);
}

static gboolean
gum_unwind_pc_translator_default_install_resume_context (
    GumUnwindPcTranslator * self,
    gpointer unwind_context,
    GumAddress real_resume_ip)
{
  return FALSE;
}

#if !defined (HAVE_DARWIN) && !defined (HAVE_LINUX)

void
_gum_unwind_broker_backend_activate (void)
{
}

void
_gum_unwind_broker_backend_deactivate (void)
{
}

#endif
