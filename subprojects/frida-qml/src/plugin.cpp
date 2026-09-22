#include <frida-core.h>

#include "plugin.h"

#include "application.h"
#include "device.h"
#include "frida.h"
#include "iconprovider.h"
#include "process.h"
#include "script.h"

#include <qqml.h>

static QObject *createFridaSingleton(QQmlEngine *engine, QJSEngine *scriptEngine)
{
    Q_UNUSED(engine);
    Q_UNUSED(scriptEngine);

    return Frida::instance();
}

void FridaQmlPlugin::registerTypes(const char *uri)
{
    qRegisterMetaType<QList<Application *>>("QList<Application *>");
    qRegisterMetaType<QList<Process *>>("QList<Process *>");
    qRegisterMetaType<QSet<unsigned int>>("QSet<unsigned int>");
    qRegisterMetaType<Device::Type>("Device::Type");
    qRegisterMetaType<SessionEntry::DetachReason>("SessionEntry::DetachReason");
    qRegisterMetaType<Script::Status>("Script::Status");
    qRegisterMetaType<Script::Runtime>("Script::Runtime");
    qRegisterMetaType<ScriptInstance::Status>("ScriptInstance::Status");

    qmlRegisterSingletonType<Frida>(uri, 1, 0, "Frida", createFridaSingleton);
}

void FridaQmlPlugin::initializeEngine(QQmlEngine *engine, const char *uri)
{
    Q_UNUSED(uri);

    // Ensure Frida is initialized.
    Frida::instance();

    engine->addImageProvider("frida", IconProvider::instance());
}
