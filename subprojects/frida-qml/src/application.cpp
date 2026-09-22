#include <frida-core.h>

#include "application.h"
#include "variant.h"

Application::Application(FridaApplication *handle, QObject *parent) :
    QObject(parent),
    m_identifier(frida_application_get_identifier(handle)),
    m_name(frida_application_get_name(handle)),
    m_pid(frida_application_get_pid(handle)),
    m_parameters(Frida::parseParametersDict(frida_application_get_parameters(handle)))
{
    auto iconProvider = IconProvider::instance();
    for (QVariant serializedIcon : m_parameters["icons"].toList())
        m_icons.append(iconProvider->add(serializedIcon.toMap()));
}

Application::~Application()
{
    auto iconProvider = IconProvider::instance();
    for (Icon icon : m_icons)
        iconProvider->remove(icon);
}

QVector<QUrl> Application::icons() const
{
    QVector<QUrl> urls;
    for (Icon icon : m_icons)
        urls.append(icon.url());
    return urls;
}
