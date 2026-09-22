#include "plugin.h"
#include <QFile>

QString plugin1::getResource()
{
    return "hello world";
}


#if QT_VERSION < 0x050000
    Q_EXPORT_PLUGIN2(Plugin1, plugin1)
#endif