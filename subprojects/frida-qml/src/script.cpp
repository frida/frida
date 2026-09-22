#include "script.h"

#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>

Script::Script(QObject *parent) :
    QObject(parent),
    m_status(Status::Loaded),
    m_runtime(Runtime::Default)
{
}

void Script::setUrl(QUrl url)
{
    if (url == m_url)
        return;

    QNetworkRequest request(url);
    auto reply = m_networkAccessManager.get(request);
    m_status = Status::Loading;
    Q_EMIT statusChanged(m_status);
    connect(reply, &QNetworkReply::finished, [=] () {
        if (m_status == Status::Loading) {
            if (reply->error() == QNetworkReply::NoError) {
                if (m_name.isEmpty()) {
                    setName(url.fileName(QUrl::FullyDecoded).section(".", 0, 0));
                }

                m_code = reply->readAll();
                Q_EMIT codeChanged(m_code);

                m_status = Status::Loaded;
                Q_EMIT statusChanged(m_status);
            } else {
                Q_EMIT error(nullptr, QString("Failed to load “").append(url.toString()).append("”"));

                m_status = Status::Error;
                Q_EMIT statusChanged(m_status);
            }
        }

        reply->deleteLater();
    });
}

void Script::setName(QString name)
{
    if (name == m_name)
        return;

    m_name = name;
    Q_EMIT nameChanged(m_name);
}

void Script::setRuntime(Runtime runtime)
{
    if (runtime == m_runtime)
        return;

    m_runtime = runtime;
    Q_EMIT runtimeChanged(m_runtime);
}

void Script::setCode(QByteArray code)
{
    m_code = code;
    Q_EMIT codeChanged(m_code);

    if (m_status == Status::Loading) {
        m_status = Status::Loaded;
        Q_EMIT statusChanged(m_status);
    }
}

void Script::resumeProcess()
{
    for (QObject *obj : std::as_const(m_instances))
        qobject_cast<ScriptInstance *>(obj)->resumeProcess();
}

void Script::stop()
{
    for (QObject *obj : std::as_const(m_instances))
        qobject_cast<ScriptInstance *>(obj)->stop();
}

void Script::post(QJsonObject object)
{
    post(static_cast<QJsonValue>(object));
}

void Script::post(QJsonArray array)
{
    post(static_cast<QJsonValue>(array));
}

void Script::post(QJsonValue value)
{
    for (QObject *obj : std::as_const(m_instances))
        qobject_cast<ScriptInstance *>(obj)->post(value);
}

void Script::enableDebugger()
{
    enableDebugger(0);
}

void Script::enableDebugger(quint16 basePort)
{
    int i = 0;
    for (QObject *obj : std::as_const(m_instances)) {
        qobject_cast<ScriptInstance *>(obj)->enableDebugger(basePort + i);
        i++;
    }
}

void Script::disableDebugger()
{
    for (QObject *obj : std::as_const(m_instances))
        qobject_cast<ScriptInstance *>(obj)->disableDebugger();
}

ScriptInstance *Script::bind(Device *device, int pid)
{
    if (pid != -1) {
        for (QObject *obj : std::as_const(m_instances)) {
            auto instance = qobject_cast<ScriptInstance *>(obj);
            if (instance->device() == device && instance->pid() == pid)
                return nullptr;
        }
    }

    auto instance = new ScriptInstance(device, pid, this);
    connect(instance, &ScriptInstance::error, [=] (QString message) {
        Q_EMIT error(instance, message);
    });
    connect(instance, &ScriptInstance::message, [=] (QJsonObject object, QVariant data) {
        Q_EMIT message(instance, object, data);
    });

    m_instances.append(instance);
    Q_EMIT instancesChanged(m_instances);

    return instance;
}

void Script::unbind(ScriptInstance *instance)
{
    m_instances.removeOne(instance);
    Q_EMIT instancesChanged(m_instances);

    instance->deleteLater();
}

ScriptInstance::ScriptInstance(Device *device, int pid, Script *parent) :
    QObject(parent),
    m_status(Status::Loading),
    m_device(device),
    m_pid(pid),
    m_processState((pid == -1) ? ProcessState::Spawning : ProcessState::Running)
{
}

void ScriptInstance::onSpawnComplete(int pid)
{
    m_pid = pid;
    m_processState = ProcessState::Paused;
    Q_EMIT pidChanged(m_pid);
    Q_EMIT processStateChanged(m_processState);
}

void ScriptInstance::onResumeComplete()
{
    m_processState = ProcessState::Running;
    Q_EMIT processStateChanged(m_processState);
}

void ScriptInstance::resumeProcess()
{
    if (m_processState != ProcessState::Paused)
        return;

    m_processState = ProcessState::Resuming;
    Q_EMIT processStateChanged(m_processState);

    Q_EMIT resumeProcessRequest();
}

void ScriptInstance::stop()
{
    if (m_status == Status::Destroyed)
        return;

    Q_EMIT stopRequest();

    m_status = Status::Destroyed;
    Q_EMIT statusChanged(m_status);
}

void ScriptInstance::post(QJsonObject object)
{
    post(static_cast<QJsonValue>(object));
}

void ScriptInstance::post(QJsonArray array)
{
    post(static_cast<QJsonValue>(array));
}

void ScriptInstance::post(QJsonValue value)
{
    Q_EMIT send(value);
}

void ScriptInstance::enableDebugger()
{
    Q_EMIT enableDebuggerRequest(0);
}

void ScriptInstance::enableDebugger(quint16 port)
{
    Q_EMIT enableDebuggerRequest(port);
}

void ScriptInstance::disableDebugger()
{
    Q_EMIT disableDebuggerRequest();
}

void ScriptInstance::onStatus(Status status)
{
    if (m_status == Status::Destroyed)
        return;

    m_status = status;
    Q_EMIT statusChanged(status);

    if (status == Status::Error)
        Q_EMIT stopRequest();
}

void ScriptInstance::onError(QString message)
{
    if (m_status == Status::Destroyed)
        return;

    Q_EMIT error(message);
}

void ScriptInstance::onMessage(QJsonObject object, QVariant data)
{
    if (m_status == Status::Destroyed)
        return;

    Q_EMIT message(object, data);
}
