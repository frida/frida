#include <frida-core.h>

#include "iconprovider.h"

#include <QMutexLocker>

IconProvider *IconProvider::s_instance = nullptr;

IconProvider::IconProvider() :
    QQuickImageProvider(QQuickImageProvider::Image),
    m_nextId(1)
{
}

IconProvider::~IconProvider()
{
    s_instance = nullptr;
}

IconProvider *IconProvider::instance()
{
    if (s_instance == nullptr)
        s_instance = new IconProvider();
    return s_instance;
}

Icon IconProvider::add(QVariantMap serializedIcon)
{
    auto id = m_nextId++;

    {
        QMutexLocker locker(&m_mutex);
        m_icons[id] = serializedIcon;
    }

    QUrl url;
    url.setScheme("image");
    url.setHost("frida");
    url.setPath(QString("/").append(QString::number(id)));

    return Icon(id, url);
}

void IconProvider::remove(Icon icon)
{
    if (!icon.isValid())
        return;

    auto id = icon.id();
    {
        QMutexLocker locker(&m_mutex);
        m_icons.remove(id);
    }
}

QImage IconProvider::requestImage(const QString &id, QSize *size, const QSize &requestedSize)
{
    auto rawId = id.toInt();

    QVariantMap serializedIcon;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_icons.contains(rawId))
            return QImage();
        serializedIcon = m_icons[rawId];
    }

    QString format = serializedIcon["format"].toString();
    if (format == "rgba") {
        int width = serializedIcon["width"].toInt();
        int height = serializedIcon["height"].toInt();
        QByteArray image = serializedIcon["image"].toByteArray();
        if (width == 0 || height == 0 || image.length() != width * height * 4)
            return QImage();

        *size = QSize(width, height);

        QImage result(width, height, QImage::Format_RGBA8888);
        memcpy(result.bits(), image.data(), image.length());

        if (requestedSize.isValid())
            return result.scaled(requestedSize, Qt::KeepAspectRatio);

        return result;
    }

    if (format == "png") {
        QImage result;
        result.loadFromData(serializedIcon["image"].toByteArray());
        return result;
    }

    return QImage();
}
