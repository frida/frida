#include <frida-core.h>

#include "device.h"

#include "maincontext.h"
#include "script.h"
#include "spawnoptions.h"
#include "variant.h"

#include <memory>
#include <QDebug>
#include <QJsonDocument>
#include <QPointer>

#define QUICKJS_BYTECODE_MAGIC 0x02

static void deleteByteArray(gpointer data);

Device::Device(FridaDevice *handle, QObject *parent) :
    QObject(parent),
    m_handle(handle),
    m_id(frida_device_get_id(handle)),
    m_name(frida_device_get_name(handle)),
    m_type(static_cast<Device::Type>(frida_device_get_dtype(handle))),
    m_gcTimer(nullptr),
    m_mainContext(new MainContext(frida_get_main_context()))
{
    auto serializedIcon = Frida::parseVariant(frida_device_get_icon(handle)).toMap();
    if (!serializedIcon.isEmpty())
        m_icon = IconProvider::instance()->add(serializedIcon);

    g_object_ref(m_handle);
    g_object_set_data(G_OBJECT(m_handle), "qdevice", this);
}

void Device::dispose()
{
    if (m_gcTimer != nullptr) {
        g_source_destroy(m_gcTimer);
        m_gcTimer = nullptr;
    }

    auto it = m_sessions.constBegin();
    while (it != m_sessions.constEnd()) {
        delete it.value();
        ++it;
    }

    g_object_set_data(G_OBJECT(m_handle), "qdevice", nullptr);
    g_object_unref(m_handle);
}

Device::~Device()
{
    IconProvider::instance()->remove(m_icon);

    m_mainContext->perform([this] () { dispose(); });
}

ScriptInstance *Device::inject(Script *script, QString program, SpawnOptions *options)
{
    ScriptInstance *instance = createScriptInstance(script, -1);
    if (instance == nullptr)
        return nullptr;

    FridaSpawnOptions *optionsHandle;
    if (options != nullptr) {
        optionsHandle = options->handle();
        g_object_ref(optionsHandle);
    } else {
        optionsHandle = nullptr;
    }

    m_mainContext->schedule([=] () { performSpawn(program, optionsHandle, instance); });

    return instance;
}

ScriptInstance *Device::inject(Script *script, int pid)
{
    ScriptInstance *instance = createScriptInstance(script, pid);
    if (instance == nullptr)
        return nullptr;

    m_mainContext->schedule([=] () { performInject(pid, instance); });

    return instance;
}

ScriptInstance *Device::createScriptInstance(Script *script, int pid)
{
    ScriptInstance *instance = (script != nullptr) ? script->bind(this, pid) : nullptr;
    if (instance == nullptr)
        return nullptr;

    QPointer<Device> device(this);
    auto onStatusChanged = std::make_shared<QMetaObject::Connection>();
    auto onResumeRequest = std::make_shared<QMetaObject::Connection>();
    auto onStopRequest = std::make_shared<QMetaObject::Connection>();
    auto onSend = std::make_shared<QMetaObject::Connection>();
    auto onEnableDebugger = std::make_shared<QMetaObject::Connection>();
    auto onDisableDebugger = std::make_shared<QMetaObject::Connection>();
    *onStatusChanged = connect(script, &Script::statusChanged, [=] () {
        tryPerformLoad(instance);
    });
    *onResumeRequest = connect(instance, &ScriptInstance::resumeProcessRequest, [=] () {
        m_mainContext->schedule([=] () { performResume(instance); });
    });
    *onStopRequest = connect(instance, &ScriptInstance::stopRequest, [=] () {
        QObject::disconnect(*onStatusChanged);
        QObject::disconnect(*onResumeRequest);
        QObject::disconnect(*onStopRequest);
        QObject::disconnect(*onSend);
        QObject::disconnect(*onEnableDebugger);
        QObject::disconnect(*onDisableDebugger);

        script->unbind(instance);

        if (!device.isNull()) {
            device->m_mainContext->schedule([=] () { device->performStop(instance); });
        }
    });
    *onSend = connect(instance, &ScriptInstance::send, [=] (QJsonValue value) {
        m_mainContext->schedule([=] () { performPost(instance, value); });
    });
    *onEnableDebugger = connect(instance, &ScriptInstance::enableDebuggerRequest, [=] (quint16 port) {
        m_mainContext->schedule([=] () { performEnableDebugger(instance, port); });
    });
    *onDisableDebugger = connect(instance, &ScriptInstance::disableDebuggerRequest, [=] () {
        m_mainContext->schedule([=] () { performDisableDebugger(instance); });
    });

    return instance;
}

void Device::performSpawn(QString program, FridaSpawnOptions *options, ScriptInstance *wrapper)
{
    std::string programStr = program.toStdString();
    frida_device_spawn(handle(), programStr.c_str(), options, nullptr, onSpawnReadyWrapper, wrapper);
    g_object_unref(options);
}

void Device::onSpawnReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data)
{
    Device *device = static_cast<Device *>(g_object_get_data(obj, "qdevice"));
    if (device != nullptr) {
        device->onSpawnReady(res, static_cast<ScriptInstance *>(data));
    }
}

void Device::onSpawnReady(GAsyncResult *res, ScriptInstance *wrapper)
{
    GError *error = nullptr;
    guint pid = frida_device_spawn_finish(handle(), res, &error);

    if (error == nullptr) {
        QMetaObject::invokeMethod(wrapper, "onSpawnComplete", Qt::QueuedConnection,
            Q_ARG(int, pid));

        performInject(pid, wrapper);
    } else {
        QMetaObject::invokeMethod(wrapper, "onError", Qt::QueuedConnection,
            Q_ARG(QString, QString::fromUtf8(error->message)));
        QMetaObject::invokeMethod(wrapper, "onStatus", Qt::QueuedConnection,
            Q_ARG(ScriptInstance::Status, ScriptInstance::Status::Error));

        g_clear_error(&error);
    }
}

void Device::performResume(ScriptInstance *wrapper)
{
    frida_device_resume(handle(), wrapper->pid(), nullptr, onResumeReadyWrapper, wrapper);
}

void Device::onResumeReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data)
{
    Device *device = static_cast<Device *>(g_object_get_data(obj, "qdevice"));
    if (device != nullptr) {
        device->onResumeReady(res, static_cast<ScriptInstance *>(data));
    }
}

void Device::onResumeReady(GAsyncResult *res, ScriptInstance *wrapper)
{
    GError *error = nullptr;
    frida_device_resume_finish(handle(), res, &error);

    if (error == nullptr) {
        QMetaObject::invokeMethod(wrapper, "onResumeComplete", Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(wrapper, "onError", Qt::QueuedConnection,
            Q_ARG(QString, QString::fromUtf8(error->message)));
        QMetaObject::invokeMethod(wrapper, "onStatus", Qt::QueuedConnection,
            Q_ARG(ScriptInstance::Status, ScriptInstance::Status::Error));

        g_clear_error(&error);
    }
}

void Device::performInject(int pid, ScriptInstance *wrapper)
{
    auto session = m_sessions[pid];
    if (session == nullptr) {
        session = new SessionEntry(this, pid);
        m_sessions[pid] = session;
        connect(session, &SessionEntry::detached, [=] () {
            for (ScriptEntry *script : session->scripts())
                m_scripts.remove(script->wrapper());
            m_sessions.remove(pid);
            m_mainContext->schedule([=] () {
                delete session;
            });
        });
    }

    auto script = session->add(wrapper);
    m_scripts[wrapper] = script;
    connect(script, &ScriptEntry::stopped, [=] () {
        m_mainContext->schedule([=] () { delete script; });
    });

    QMetaObject::invokeMethod(this, "tryPerformLoad", Qt::QueuedConnection,
        Q_ARG(ScriptInstance *, wrapper));
}

void Device::tryPerformLoad(ScriptInstance *wrapper)
{
    Script *script = reinterpret_cast<Script *>(wrapper->parent());
    if (script->status() != Script::Status::Loaded)
        return;

    auto name = script->name();
    auto runtime = script->runtime();
    auto code = script->code();
    m_mainContext->schedule([=] () { performLoad(wrapper, name, runtime, code); });
}

void Device::performLoad(ScriptInstance *wrapper, QString name, Script::Runtime runtime, QByteArray code)
{
    auto script = m_scripts[wrapper];
    if (script == nullptr)
        return;
    script->load(name, runtime, code);
}

void Device::performStop(ScriptInstance *wrapper)
{
    auto script = m_scripts[wrapper];
    if (script == nullptr)
        return;
    m_scripts.remove(wrapper);

    script->session()->remove(script);

    scheduleGarbageCollect();
}

void Device::performPost(ScriptInstance *wrapper, QJsonValue value)
{
    auto script = m_scripts[wrapper];
    if (script == nullptr)
        return;
    script->post(value);
}

void Device::performEnableDebugger(ScriptInstance *wrapper, quint16 port)
{
    auto script = m_scripts[wrapper];
    if (script == nullptr)
        return;
    script->enableDebugger(port);
}

void Device::performDisableDebugger(ScriptInstance *wrapper)
{
    auto script = m_scripts[wrapper];
    if (script == nullptr)
        return;
    script->disableDebugger();
}

void Device::scheduleGarbageCollect()
{
    if (m_gcTimer != nullptr) {
        g_source_destroy(m_gcTimer);
        m_gcTimer = nullptr;
    }

    auto timer = g_timeout_source_new_seconds(5);
    g_source_set_callback(timer, onGarbageCollectTimeoutWrapper, this, nullptr);
    g_source_attach(timer, m_mainContext->handle());
    g_source_unref(timer);
    m_gcTimer = timer;
}

gboolean Device::onGarbageCollectTimeoutWrapper(gpointer data)
{
    static_cast<Device *>(data)->onGarbageCollectTimeout();

    return FALSE;
}

void Device::onGarbageCollectTimeout()
{
    m_gcTimer = nullptr;

    auto newSessions = QHash<int, SessionEntry *>();
    auto it = m_sessions.constBegin();
    while (it != m_sessions.constEnd()) {
        auto pid = it.key();
        auto session = it.value();
        if (session->scripts().isEmpty()) {
            delete session;
        } else {
            newSessions[pid] = session;
        }
        ++it;
    }
    m_sessions = newSessions;
}

SessionEntry::SessionEntry(Device *device, int pid, QObject *parent) :
    QObject(parent),
    m_device(device),
    m_pid(pid),
    m_handle(nullptr)
{
    frida_device_attach(device->handle(), pid, nullptr, nullptr, onAttachReadyWrapper, this);
}

SessionEntry::~SessionEntry()
{
    if (m_handle != nullptr) {
        frida_session_detach(m_handle, nullptr, nullptr, nullptr);

        g_signal_handlers_disconnect_by_func(m_handle, GSIZE_TO_POINTER(onDetachedWrapper), this);

        g_object_set_data(G_OBJECT(m_handle), "qsession", nullptr);
        g_object_unref(m_handle);
    }
}

ScriptEntry *SessionEntry::add(ScriptInstance *wrapper)
{
    auto script = new ScriptEntry(this, wrapper, this);
    m_scripts.append(script);
    script->updateSessionHandle(m_handle);
    return script;
}

void SessionEntry::remove(ScriptEntry *script)
{
    script->stop();
    m_scripts.removeOne(script);
}

void SessionEntry::onAttachReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data)
{
    if (g_object_get_data(obj, "qdevice") != nullptr) {
        static_cast<SessionEntry *>(data)->onAttachReady(res);
    }
}

void SessionEntry::onAttachReady(GAsyncResult *res)
{
    GError *error = nullptr;
    m_handle = frida_device_attach_finish(m_device->handle(), res, &error);
    if (error == nullptr) {
        g_object_set_data(G_OBJECT(m_handle), "qsession", this);

        g_signal_connect_swapped(m_handle, "detached", G_CALLBACK(onDetachedWrapper), this);

        for (ScriptEntry *script : std::as_const(m_scripts)) {
            script->updateSessionHandle(m_handle);
        }
    } else {
        for (ScriptEntry *script : std::as_const(m_scripts)) {
            script->notifySessionError(error);
        }
        g_clear_error(&error);
    }
}

void SessionEntry::onDetachedWrapper(SessionEntry *self, int reason, FridaCrash * crash)
{
    Q_UNUSED(crash);

    self->onDetached(static_cast<DetachReason>(reason));
}

void SessionEntry::onDetached(DetachReason reason)
{
    const char *message;
    switch (reason) {
    case DetachReason::ApplicationRequested:
        message = "Detached by application";
        break;
    case DetachReason::ProcessReplaced:
        message = "Process replaced";
        break;
    case DetachReason::ProcessTerminated:
        message = "Process terminated";
        break;
    case DetachReason::ConnectionTerminated:
        message = "Connection terminated";
        break;
    case DetachReason::DeviceLost:
        message = "Device lost";
        break;
    default:
        g_assert_not_reached();
    }

    for (ScriptEntry *script : std::as_const(m_scripts))
        script->notifySessionError(message);

    Q_EMIT detached(reason);
}

ScriptEntry::ScriptEntry(SessionEntry *session, ScriptInstance *wrapper, QObject *parent) :
    QObject(parent),
    m_status(ScriptInstance::Status::Loading),
    m_session(session),
    m_wrapper(wrapper),
    m_runtime(Script::Runtime::Default),
    m_handle(nullptr),
    m_sessionHandle(nullptr)
{
}

ScriptEntry::~ScriptEntry()
{
    if (m_handle != nullptr) {
        frida_script_unload(m_handle, nullptr, nullptr, nullptr);

        g_signal_handlers_disconnect_by_func(m_handle, GSIZE_TO_POINTER(onMessage), this);

        g_object_set_data(G_OBJECT(m_handle), "qscript", nullptr);
        g_object_unref(m_handle);
    }
}

void ScriptEntry::updateSessionHandle(FridaSession *sessionHandle)
{
    m_sessionHandle = sessionHandle;
    start();
}

void ScriptEntry::notifySessionError(GError *error)
{
    updateError(error);
    updateStatus(ScriptInstance::Status::Error);
}

void ScriptEntry::notifySessionError(QString message)
{
    updateError(message);
    updateStatus(ScriptInstance::Status::Error);
}

void ScriptEntry::post(QJsonValue value)
{
    if (m_status == ScriptInstance::Status::Started) {
        performPost(value);
    } else if (m_status < ScriptInstance::Status::Started) {
        m_pending.enqueue(value);
    } else {
        // Drop silently
    }
}

void ScriptEntry::enableDebugger(quint16 port)
{
  if (m_handle == nullptr)
    return;

  frida_script_enable_debugger(m_handle, port, nullptr, nullptr, nullptr);
}

void ScriptEntry::disableDebugger()
{
  if (m_handle == nullptr)
    return;

  frida_script_disable_debugger(m_handle, nullptr, nullptr, nullptr);
}

void ScriptEntry::updateStatus(ScriptInstance::Status status)
{
    if (status == m_status)
        return;

    m_status = status;

    QMetaObject::invokeMethod(m_wrapper, "onStatus", Qt::QueuedConnection,
        Q_ARG(ScriptInstance::Status, status));

    if (status == ScriptInstance::Status::Started) {
        while (!m_pending.isEmpty())
            performPost(m_pending.dequeue());
    } else if (status > ScriptInstance::Status::Started) {
        m_pending.clear();
    }
}

void ScriptEntry::updateError(GError *error)
{
    updateError(QString::fromUtf8(error->message));
}

void ScriptEntry::updateError(QString message)
{
    QMetaObject::invokeMethod(m_wrapper, "onError", Qt::QueuedConnection,
        Q_ARG(QString, message));
}

void ScriptEntry::load(QString name, Script::Runtime runtime, QByteArray code)
{
    if (m_status != ScriptInstance::Status::Loading)
        return;

    m_name = name;
    m_runtime = runtime;
    m_code = code;
    updateStatus(ScriptInstance::Status::Loaded);

    start();
}

void ScriptEntry::start()
{
    if (m_status == ScriptInstance::Status::Loading)
        return;

    if (m_sessionHandle != nullptr) {
        updateStatus(ScriptInstance::Status::Compiling);

        QByteArray code = m_code;

        auto options = frida_script_options_new();

        if (!m_name.isEmpty()) {
            std::string name = m_name.toStdString();
            frida_script_options_set_name(options, name.c_str());
        }

        frida_script_options_set_runtime(options, static_cast<FridaScriptRuntime>(m_runtime));

        if (m_code.startsWith(QUICKJS_BYTECODE_MAGIC)) {
            QByteArray *code = new QByteArray(m_code);
            GBytes *bytes = g_bytes_new_with_free_func(code->data(), code->size(), deleteByteArray, code);
            frida_session_create_script_from_bytes(m_sessionHandle, bytes, options, nullptr,
                onCreateFromBytesReadyWrapper, this);
        } else {
            std::string source = QString::fromUtf8(m_code).toStdString();
            frida_session_create_script(m_sessionHandle, source.c_str(), options, nullptr,
                onCreateFromSourceReadyWrapper, this);
        }

        g_object_unref(options);
    } else {
        updateStatus(ScriptInstance::Status::Establishing);
    }
}

static void deleteByteArray(gpointer data)
{
    QByteArray *array = static_cast<QByteArray *>(data);
    delete array;
}

void ScriptEntry::stop()
{
    bool canStopNow = m_status != ScriptInstance::Status::Compiling && m_status != ScriptInstance::Status::Starting;

    m_status = ScriptInstance::Status::Destroyed;

    if (canStopNow)
        Q_EMIT stopped();
}

void ScriptEntry::onCreateFromSourceReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data)
{
    if (g_object_get_data(obj, "qsession") != nullptr) {
        static_cast<ScriptEntry *>(data)->onCreateFromSourceReady(res);
    }
}

void ScriptEntry::onCreateFromSourceReady(GAsyncResult *res)
{
    GError *error = nullptr;
    FridaScript *handle = frida_session_create_script_finish(m_sessionHandle, res, &error);
    onCreateComplete(&handle, &error);
}

void ScriptEntry::onCreateFromBytesReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data)
{
    if (g_object_get_data(obj, "qsession") != nullptr) {
        static_cast<ScriptEntry *>(data)->onCreateFromBytesReady(res);
    }
}

void ScriptEntry::onCreateFromBytesReady(GAsyncResult *res)
{
    GError *error = nullptr;
    FridaScript *handle = frida_session_create_script_from_bytes_finish(m_sessionHandle, res, &error);
    onCreateComplete(&handle, &error);
}

void ScriptEntry::onCreateComplete(FridaScript **handle, GError **error)
{
    if (m_status == ScriptInstance::Status::Destroyed) {
        g_clear_object(handle);
        g_clear_error(error);

        Q_EMIT stopped();
        return;
    }

    if (*error == nullptr) {
        m_handle = static_cast<FridaScript *>(g_steal_pointer(handle));
        g_object_set_data(G_OBJECT(m_handle), "qscript", this);

        g_signal_connect_swapped(m_handle, "message", G_CALLBACK(onMessage), this);

        updateStatus(ScriptInstance::Status::Starting);
        frida_script_load(m_handle, nullptr, onLoadReadyWrapper, this);
    } else {
        updateError(*error);
        updateStatus(ScriptInstance::Status::Error);
        g_clear_error(error);
    }
}

void ScriptEntry::onLoadReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data)
{
    if (g_object_get_data(obj, "qscript") != nullptr) {
        static_cast<ScriptEntry *>(data)->onLoadReady(res);
    }
}

void ScriptEntry::onLoadReady(GAsyncResult *res)
{
    GError *error = nullptr;
    frida_script_load_finish(m_handle, res, &error);

    if (m_status == ScriptInstance::Status::Destroyed) {
        g_clear_error(&error);

        Q_EMIT stopped();
        return;
    }

    if (error == nullptr) {
        updateStatus(ScriptInstance::Status::Started);
    } else {
        updateError(error);
        updateStatus(ScriptInstance::Status::Error);
        g_clear_error(&error);
    }
}

void ScriptEntry::performPost(QJsonValue value)
{
    QJsonDocument document = value.isObject()
        ? QJsonDocument(value.toObject())
        : QJsonDocument(value.toArray());
    auto json = document.toJson(QJsonDocument::Compact);
    frida_script_post(m_handle, json.data(), nullptr);
}

void ScriptEntry::onMessage(ScriptEntry *self, const gchar *message, GBytes *data)
{
    auto messageJson = QByteArray::fromRawData(message, static_cast<int>(strlen(message)));
    auto messageDocument = QJsonDocument::fromJson(messageJson);
    auto messageObject = messageDocument.object();

    if (messageObject["type"] == "log") {
        std::string logMessage = messageObject["payload"].toString().toStdString();
        qDebug("%s", logMessage.c_str());
    } else {
        QVariant dataValue;
        if (data != nullptr) {
            gsize dataSize;
            auto dataBuffer = static_cast<const char *>(g_bytes_get_data(data, &dataSize));
            dataValue = QByteArray(dataBuffer, dataSize);
        }

        QMetaObject::invokeMethod(self->m_wrapper, "onMessage", Qt::QueuedConnection,
            Q_ARG(QJsonObject, messageDocument.object()),
            Q_ARG(QVariant, dataValue));
    }
}
