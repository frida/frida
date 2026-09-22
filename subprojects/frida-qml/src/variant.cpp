#include "variant.h"

namespace Frida
{
    QVariantMap parseParametersDict(GHashTable *dict)
    {
        QVariantMap result;

        GHashTableIter iter;
        g_hash_table_iter_init(&iter, dict);

        gpointer rawKey, rawValue;
        while (g_hash_table_iter_next(&iter, &rawKey, &rawValue)) {
            auto key = static_cast<const gchar *>(rawKey);
            auto value = static_cast<GVariant *>(rawValue);
            result[key] = parseVariant(value);
        }

        return result;
    }

    QVariant parseVariant(GVariant *v)
    {
        if (v == nullptr)
            return QVariant();

        if (g_variant_is_of_type(v, G_VARIANT_TYPE_STRING))
            return QVariant(g_variant_get_string(v, nullptr));

        if (g_variant_is_of_type(v, G_VARIANT_TYPE_INT64))
            return QVariant(static_cast<qlonglong>(g_variant_get_int64(v)));

        if (g_variant_is_of_type(v, G_VARIANT_TYPE_BOOLEAN))
            return QVariant(g_variant_get_boolean(v) != FALSE);

        if (g_variant_is_of_type(v, G_VARIANT_TYPE("ay"))) {
            gsize size;
            gconstpointer data = g_variant_get_fixed_array(v, &size, sizeof(guint8));
            return QVariant(QByteArray(static_cast<const char *>(data), size));
        }

        if (g_variant_is_of_type(v, G_VARIANT_TYPE_VARDICT)) {
            QVariantMap result;

            GVariantIter iter;
            g_variant_iter_init(&iter, v);

            gchar *key;
            GVariant *value;
            while (g_variant_iter_next(&iter, "{sv}", &key, &value)) {
                result[key] = parseVariant(value);
                g_variant_unref(value);
                g_free(key);
            }

            return result;
        }

        if (g_variant_is_of_type(v, G_VARIANT_TYPE_ARRAY)) {
            QVariantList result;

            GVariantIter iter;
            g_variant_iter_init(&iter, v);

            GVariant *value;
            while ((value = g_variant_iter_next_value(&iter)) != nullptr) {
                result.append(parseVariant(value));
                g_variant_unref(value);
            }

            return result;
        }

        return QVariant();
    }
};
