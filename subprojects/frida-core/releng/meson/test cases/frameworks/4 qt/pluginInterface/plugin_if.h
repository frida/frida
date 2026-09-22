#ifndef PLUGIN_IF_H
#define PLUGIN_IF_H

#include <QString>
#include <QtPlugin>

/**
 * @brief Interface for a plugin
 */
class PluginInterface
{
public:
    virtual ~PluginInterface() = default;

    /// Initializes the plugin
    virtual QString getResource() = 0;
};

Q_DECLARE_INTERFACE(PluginInterface, "demo.PluginInterface")

#endif
