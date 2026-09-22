#ifndef FRIDAQML_PROCESS_H
#define FRIDAQML_PROCESS_H

#include "fridafwd.h"
#include "iconprovider.h"

#include <QObject>

class Process : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Process)
    Q_PROPERTY(unsigned int pid READ pid CONSTANT)
    Q_PROPERTY(QString name READ name CONSTANT)
    Q_PROPERTY(QVariantMap parameters READ parameters CONSTANT)
    Q_PROPERTY(QVector<QUrl> icons READ icons CONSTANT)
    QML_ELEMENT
    QML_UNCREATABLE("Process objects cannot be instantiated from Qml");

public:
    explicit Process(FridaProcess *handle, QObject *parent = nullptr);
    ~Process();

    unsigned int pid() const { return m_pid; }
    QString name() const { return m_name; }
    QVariantMap parameters() const { return m_parameters; }
    bool hasIcons() const { return !m_icons.empty(); }
    QVector<QUrl> icons() const;

private:
    unsigned int m_pid;
    QString m_name;
    QVariantMap m_parameters;
    QVector<Icon> m_icons;
};

#endif
