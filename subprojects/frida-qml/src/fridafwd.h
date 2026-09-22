#ifndef FRIDAQML_FRIDAFWD_H
#define FRIDAQML_FRIDAFWD_H

// FIXME: Investigate compatibility issue between Qt and Frida headers.

extern "C"
{
    typedef struct _FridaApplication FridaApplication;
    typedef struct _FridaCrash FridaCrash;
    typedef struct _FridaDevice FridaDevice;
    typedef struct _FridaDeviceManager FridaDeviceManager;
    typedef struct _FridaIcon FridaIcon;
    typedef struct _FridaProcess FridaProcess;
    typedef struct _FridaScript FridaScript;
    typedef struct _FridaSession FridaSession;
    typedef struct _FridaSpawnOptions FridaSpawnOptions;

    typedef struct _GAsyncResult GAsyncResult;
    typedef struct _GBytes GBytes;
    typedef struct _GError GError;
    typedef struct _GObject GObject;
    typedef struct _GSource GSource;

    typedef void *gpointer;
    typedef int gint;
    typedef gint gboolean;
    typedef char gchar;
}

#endif
