#ifndef FRIDAQML_DEVICE_H
#define FRIDAQML_DEVICE_H

#include "fridafwd.h"
#include "iconprovider.h"
#include "script.h"

#include <QHash>
#include <QObject>
#include <QQueue>

class MainContext;
class ScriptEntry;
class SessionEntry;
Q_MOC_INCLUDE("spawnoptions.h")
class SpawnOptions;

class Device : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Device)
    Q_PROPERTY(QString id READ id NOTIFY idChanged)
    Q_PROPERTY(QString name READ name NOTIFY nameChanged)
    Q_PROPERTY(Type type READ type NOTIFY typeChanged)
    QML_ELEMENT
    QML_UNCREATABLE("Device objects cannot be instantiated from Qml");

public:
    enum class Type { Local, Remote, Usb };
    Q_ENUM(Type)

    explicit Device(FridaDevice *handle, QObject *parent = nullptr);
private:
    void dispose();
public:
    ~Device();

    FridaDevice *handle() const { return m_handle; }
    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QUrl icon() const { return m_icon.url(); }
    Type type() const { return m_type; }

    Q_INVOKABLE ScriptInstance *inject(Script *script, QString program, SpawnOptions *options = nullptr);
    Q_INVOKABLE ScriptInstance *inject(Script *script, int pid);

Q_SIGNALS:
    void idChanged(QString newId);
    void nameChanged(QString newName);
    void typeChanged(Type newType);

private:
    ScriptInstance *createScriptInstance(Script *script, int pid);
    void performSpawn(QString program, FridaSpawnOptions *options, ScriptInstance *wrapper);
    static void onSpawnReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data);
    void onSpawnReady(GAsyncResult *res, ScriptInstance *wrapper);
    void performResume(ScriptInstance *wrapper);
    static void onResumeReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data);
    void onResumeReady(GAsyncResult *res, ScriptInstance *wrapper);
    void performInject(int pid, ScriptInstance *wrapper);
private Q_SLOTS:
    void tryPerformLoad(ScriptInstance *wrapper);
private:
    void performLoad(ScriptInstance *wrapper, QString name, Script::Runtime runtime, QByteArray code);
    void performStop(ScriptInstance *wrapper);
    void performPost(ScriptInstance *wrapper, QJsonValue value);
    void performEnableDebugger(ScriptInstance *wrapper, quint16 port);
    void performDisableDebugger(ScriptInstance *wrapper);
    void scheduleGarbageCollect();
    static gboolean onGarbageCollectTimeoutWrapper(gpointer data);
    void onGarbageCollectTimeout();

    FridaDevice *m_handle;
    QString m_id;
    QString m_name;
    Icon m_icon;
    Type m_type;

    QHash<int, SessionEntry *> m_sessions;
    QHash<ScriptInstance *, ScriptEntry *> m_scripts;
    GSource *m_gcTimer;

    QScopedPointer<MainContext> m_mainContext;
};

class SessionEntry : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SessionEntry)

public:
    enum class DetachReason {
        ApplicationRequested = 1,
        ProcessReplaced,
        ProcessTerminated,
        ConnectionTerminated,
        DeviceLost
    };
    Q_ENUM(DetachReason)

    explicit SessionEntry(Device *device, int pid, QObject *parent = nullptr);
    ~SessionEntry();

    QList<ScriptEntry *> scripts() const { return m_scripts; }

    ScriptEntry *add(ScriptInstance *wrapper);
    void remove(ScriptEntry *script);

Q_SIGNALS:
    void detached(DetachReason reason);

private:
    static void onAttachReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data);
    void onAttachReady(GAsyncResult *res);
    static void onDetachedWrapper(SessionEntry *self, int reason, FridaCrash * crash);
    void onDetached(DetachReason reason);

    Device *m_device;
    int m_pid;
    FridaSession *m_handle;
    QList<ScriptEntry *> m_scripts;
};

class ScriptEntry : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ScriptEntry)

public:
    explicit ScriptEntry(SessionEntry *session, ScriptInstance *wrapper, QObject *parent = nullptr);
    ~ScriptEntry();

    SessionEntry *session() const { return m_session; }
    ScriptInstance *wrapper() const { return m_wrapper; }

    void updateSessionHandle(FridaSession *sessionHandle);
    void notifySessionError(GError *error);
    void notifySessionError(QString message);
    void load(QString name, Script::Runtime runtime, QByteArray code);
    void stop();
    void post(QJsonValue value);
    void enableDebugger(quint16 port);
    void disableDebugger();

Q_SIGNALS:
    void stopped();

private:
    void updateStatus(ScriptInstance::Status status);
    void updateError(GError *error);
    void updateError(QString message);

    void start();
    static void onCreateFromSourceReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data);
    void onCreateFromSourceReady(GAsyncResult *res);
    static void onCreateFromBytesReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data);
    void onCreateFromBytesReady(GAsyncResult *res);
    void onCreateComplete(FridaScript **handle, GError **error);
    static void onLoadReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data);
    void onLoadReady(GAsyncResult *res);
    void performPost(QJsonValue value);
    static void onMessage(ScriptEntry *self, const gchar *message, GBytes *data);

    ScriptInstance::Status m_status;
    SessionEntry *m_session;
    ScriptInstance *m_wrapper;
    QString m_name;
    Script::Runtime m_runtime;
    QByteArray m_code;
    FridaScript *m_handle;
    FridaSession *m_sessionHandle;
    QQueue<QJsonValue> m_pending;
};

#endif
