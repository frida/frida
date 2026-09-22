#ifndef FRIDAQML_PLUGIN_H
#define FRIDAQML_PLUGIN_H

#include <QQmlExtensionPlugin>

class FridaQmlPlugin : public QQmlExtensionPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)

public:
    void registerTypes(const char *uri) override;
    void initializeEngine(QQmlEngine *engine, const char *uri) override;
};

#endif
