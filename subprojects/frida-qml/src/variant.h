#ifndef FRIDAQML_VARIANT_H
#define FRIDAQML_VARIANT_H

#include <frida-core.h>
#include <QVariantMap>

namespace Frida
{
    QVariantMap parseParametersDict(GHashTable *dict);
    QVariant parseVariant(GVariant *v);
};

#endif
