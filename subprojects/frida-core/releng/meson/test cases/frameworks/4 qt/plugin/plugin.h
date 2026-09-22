#pragma once
#include <plugin_if.h>

class plugin1:public QObject,public PluginInterface
{
    Q_OBJECT
    Q_INTERFACES(PluginInterface)
#if QT_VERSION >= 0x050000
    Q_PLUGIN_METADATA(IID "demo.PluginInterface" FILE "plugin.json")
#endif

public:
    QString getResource() override;
};
