#ifndef FRIDAQML_SPAWNOPTIONS_H
#define FRIDAQML_SPAWNOPTIONS_H

#include "fridafwd.h"

#include <QQmlEngine>

class SpawnOptions : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SpawnOptions)

    Q_PROPERTY(bool hasArgv READ hasArgv NOTIFY hasArgvChanged)
    Q_PROPERTY(QVector<QString> argv READ argv WRITE setArgv NOTIFY argvChanged)

    Q_PROPERTY(bool hasEnv READ hasEnv NOTIFY hasEnvChanged)
    Q_PROPERTY(QVector<QString> env READ env WRITE setEnv NOTIFY envChanged)

    Q_PROPERTY(bool hasCwd READ hasCwd NOTIFY hasCwdChanged)
    Q_PROPERTY(QString cwd READ cwd WRITE setCwd NOTIFY cwdChanged)

    QML_ELEMENT

public:
    explicit SpawnOptions(QObject *parent = nullptr);
    ~SpawnOptions();

    FridaSpawnOptions *handle() const { return m_handle; }

    bool hasArgv() const;
    QVector<QString> argv() const;
    void setArgv(QVector<QString> argv);
    Q_INVOKABLE void unsetArgv();

    bool hasEnv() const;
    QVector<QString> env() const;
    void setEnv(QVector<QString> env);
    Q_INVOKABLE void unsetEnv();

    bool hasCwd() const;
    QString cwd() const;
    void setCwd(QString cwd);
    Q_INVOKABLE void unsetCwd();

Q_SIGNALS:
    void hasArgvChanged(bool newHasArgv);
    void argvChanged(QVector<QString> newArgv);

    void hasEnvChanged(bool newHasEnv);
    void envChanged(QVector<QString> newEnv);

    void hasCwdChanged(bool newHasCwd);
    void cwdChanged(QString newCwd);

private:
    FridaSpawnOptions *m_handle;
};

#endif
