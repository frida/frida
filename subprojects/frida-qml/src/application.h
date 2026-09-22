#ifndef FRIDAQML_APPLICATION_H
#define FRIDAQML_APPLICATION_H

#include "fridafwd.h"
#include "iconprovider.h"

#include <QObject>

class Application : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Application)
    Q_PROPERTY(QString identifier READ identifier CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(unsigned int pid READ pid CONSTANT)
    Q_PROPERTY(QVariantMap parameters READ parameters CONSTANT)
    Q_PROPERTY(QVector<QUrl> icons READ icons CONSTANT)
    QML_ELEMENT
    QML_UNCREATABLE("Application objects cannot be instantiated from Qml");

public:
    explicit Application(FridaApplication *handle, QObject *parent = nullptr);
    ~Application();

    QString identifier() const { return m_identifier; }
    QString name() const { return m_name; }
    unsigned int pid() const { return m_pid; }
    QVariantMap parameters() const { return m_parameters; }
    bool hasIcons() const { return !m_icons.empty(); }
    QVector<QUrl> icons() const;

private:
    QString m_identifier;
    QString m_name;
    unsigned int m_pid;
    QVariantMap m_parameters;
    QVector<Icon> m_icons;
};

#endif
