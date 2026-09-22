#ifndef FRIDAQML_FRIDA_H
#define FRIDAQML_FRIDA_H

#include "fridafwd.h"

#include <QMutex>
#include <QQmlEngine>
#include <QWaitCondition>

Q_MOC_INCLUDE("device.h")
class Device;
class MainContext;
class Scripts;

class Frida : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(Frida)
    Q_PROPERTY(Device *localSystem READ localSystem CONSTANT)
    QML_ELEMENT
    QML_SINGLETON

public:
    enum class Scope {
        Minimal,
        Metadata,
        Full
    };
    Q_ENUM(Scope)

    explicit Frida(QObject *parent = nullptr);
private:
    void initialize();
    void dispose();
public:
    ~Frida();

    static Frida *instance();

    Device *localSystem() const { return m_localSystem; }

    QList<Device *> deviceItems() const { return m_deviceItems; }

Q_SIGNALS:
    void localSystemChanged(Device *newLocalSystem);
    void deviceAdded(Device *device);
    void deviceRemoved(Device *device);

private:
    static void onGetLocalDeviceReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data);
    void onGetLocalDeviceReady(GAsyncResult *res);
    static void onEnumerateDevicesReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data);
    void onEnumerateDevicesReady(GAsyncResult *res);
    static void onDeviceAddedWrapper(Frida *self, FridaDevice *deviceHandle);
    static void onDeviceRemovedWrapper(Frida *self, FridaDevice *deviceHandle);
    void onDeviceAdded(FridaDevice *deviceHandle);
    void onDeviceRemoved(FridaDevice *deviceHandle);

private Q_SLOTS:
    void add(Device *device);
    void removeById(QString id);

private:
    QMutex m_mutex;
    FridaDeviceManager *m_handle;
    QList<Device *> m_deviceItems;
    Device *m_localSystem;
    QWaitCondition m_localSystemAvailable;
    QScopedPointer<MainContext> m_mainContext;

    static Frida *s_instance;
};

#endif
