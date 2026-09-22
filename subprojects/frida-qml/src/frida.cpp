#include <frida-core.h>

#include "frida.h"

#include "device.h"
#include "devicelistmodel.h"
#include "maincontext.h"

Frida *Frida::s_instance = nullptr;

Frida::Frida(QObject *parent) :
    QObject(parent),
    m_localSystem(nullptr),
    m_mainContext(nullptr)
{
    frida_init();

    m_mainContext.reset(new MainContext(frida_get_main_context()));
    m_mainContext->schedule([this] () { initialize(); });

    QMutexLocker locker(&m_mutex);
    while (m_localSystem == nullptr)
        m_localSystemAvailable.wait(&m_mutex);
}

void Frida::initialize()
{
    m_handle = frida_device_manager_new();

    frida_device_manager_get_device_by_type(m_handle, FRIDA_DEVICE_TYPE_LOCAL, 0, nullptr,
        onGetLocalDeviceReadyWrapper, this);

    g_signal_connect_swapped(m_handle, "added", G_CALLBACK(onDeviceAddedWrapper), this);
    g_signal_connect_swapped(m_handle, "removed", G_CALLBACK(onDeviceRemovedWrapper), this);
    frida_device_manager_enumerate_devices(m_handle, nullptr, onEnumerateDevicesReadyWrapper, this);
}

void Frida::dispose()
{
    g_signal_handlers_disconnect_by_func(m_handle, GSIZE_TO_POINTER(onDeviceRemovedWrapper), this);
    g_signal_handlers_disconnect_by_func(m_handle, GSIZE_TO_POINTER(onDeviceAddedWrapper), this);
    g_object_unref(m_handle);
    m_handle = nullptr;
}

Frida::~Frida()
{
    m_localSystem = nullptr;
    qDeleteAll(m_deviceItems);
    m_deviceItems.clear();

    frida_device_manager_close_sync(m_handle, nullptr, nullptr);
    m_mainContext->perform([this] () { dispose(); });
    m_mainContext.reset();

    s_instance = nullptr;

    frida_deinit();
}

Frida *Frida::instance()
{
    if (s_instance == nullptr)
        s_instance = new Frida();
    return s_instance;
}

void Frida::onGetLocalDeviceReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data)
{
    Q_UNUSED(obj);

    static_cast<Frida *>(data)->onGetLocalDeviceReady(res);
}

void Frida::onGetLocalDeviceReady(GAsyncResult *res)
{
    GError *error = nullptr;
    FridaDevice *deviceHandle = frida_device_manager_get_device_by_type_finish(m_handle, res, &error);
    g_assert(error == nullptr);

    auto device = new Device(deviceHandle);
    device->moveToThread(this->thread());

    {
        QMutexLocker locker(&m_mutex);
        m_localSystem = device;
        m_localSystemAvailable.wakeOne();
    }

    QMetaObject::invokeMethod(this, "add", Qt::QueuedConnection, Q_ARG(Device *, device));

    g_object_unref(deviceHandle);
}

void Frida::onEnumerateDevicesReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data)
{
    Q_UNUSED(obj);

    static_cast<Frida *>(data)->onEnumerateDevicesReady(res);
}

void Frida::onEnumerateDevicesReady(GAsyncResult *res)
{
    GError *error = nullptr;
    FridaDeviceList *devices = frida_device_manager_enumerate_devices_finish(m_handle, res, &error);
    g_assert(error == nullptr);

    gint count = frida_device_list_size(devices);
    for (gint i = 0; i != count; i++) {
        FridaDevice *device = frida_device_list_get(devices, i);
        onDeviceAdded(device);
        g_object_unref(device);
    }

    g_object_unref(devices);
}

void Frida::onDeviceAddedWrapper(Frida *self, FridaDevice *deviceHandle)
{
    self->onDeviceAdded(deviceHandle);
}

void Frida::onDeviceRemovedWrapper(Frida *self, FridaDevice *deviceHandle)
{
    self->onDeviceRemoved(deviceHandle);
}

void Frida::onDeviceAdded(FridaDevice *deviceHandle)
{
    if (deviceHandle == m_localSystem->handle())
        return;

    auto device = new Device(deviceHandle);
    device->moveToThread(this->thread());

    QMetaObject::invokeMethod(this, "add", Qt::QueuedConnection, Q_ARG(Device *, device));
}

void Frida::onDeviceRemoved(FridaDevice *deviceHandle)
{
    QMetaObject::invokeMethod(this, "removeById", Qt::QueuedConnection, Q_ARG(QString, frida_device_get_id(deviceHandle)));
}

void Frida::add(Device *device)
{
    device->setParent(this);
    m_deviceItems.append(device);
    Q_EMIT deviceAdded(device);
}

void Frida::removeById(QString id)
{
    for (int i = 0; i != m_deviceItems.size(); i++) {
        auto device = m_deviceItems.at(i);
        if (device->id() == id) {
            m_deviceItems.removeAt(i);
            Q_EMIT deviceRemoved(device);
            delete device;
            break;
        }
    }
}
