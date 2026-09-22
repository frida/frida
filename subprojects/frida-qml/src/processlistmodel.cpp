#include <frida-core.h>

#include "processlistmodel.h"

#include "device.h"
#include "maincontext.h"
#include "process.h"

#include <QMetaMethod>

static const int ProcessPidRole = Qt::UserRole + 0;
static const int ProcessNameRole = Qt::UserRole + 1;
static const int ProcessIconsRole = Qt::UserRole + 2;

struct EnumerateProcessesRequest
{
    ProcessListModel *model;
    FridaDevice *handle;
};

ProcessListModel::ProcessListModel(QObject *parent) :
    QAbstractListModel(parent),
    m_isLoading(false),
    m_scope(Frida::Scope::Minimal),
    m_pendingRequest(nullptr),
    m_mainContext(new MainContext(frida_get_main_context()))
{
}

void ProcessListModel::dispose()
{
    if (m_pendingRequest != nullptr) {
        m_pendingRequest->model = nullptr;
        m_pendingRequest = nullptr;
    }
}

ProcessListModel::~ProcessListModel()
{
    m_mainContext->perform([this] () { dispose(); });
}

Process *ProcessListModel::get(int index) const
{
    if (index < 0 || index >= m_processes.size())
        return nullptr;

    return m_processes[index];
}

void ProcessListModel::refresh()
{
    if (m_device.isNull())
        return;

    auto handle = m_device->handle();
    g_object_ref(handle);

    auto scope = static_cast<FridaScope>(m_scope);

    m_mainContext->schedule([this, handle, scope] () { enumerateProcesses(handle, scope); });
}

Device *ProcessListModel::device() const
{
    return m_device;
}

void ProcessListModel::setDevice(Device *device)
{
    if (device == m_device)
        return;

    m_device = device;
    Q_EMIT deviceChanged(device);

    hardRefresh();
}

void ProcessListModel::setScope(Frida::Scope scope)
{
    if (scope == m_scope)
        return;

    m_scope = scope;
    Q_EMIT scopeChanged(scope);

    hardRefresh();
}

QHash<int, QByteArray> ProcessListModel::roleNames() const
{
    QHash<int, QByteArray> r;
    r[Qt::DisplayRole] = "display";
    r[ProcessPidRole] = "pid";
    r[ProcessNameRole] = "name";
    r[ProcessIconsRole] = "icons";
    return r;
}

int ProcessListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return m_processes.size();
}

QVariant ProcessListModel::data(const QModelIndex &index, int role) const
{
    auto process = m_processes[index.row()];
    switch (role) {
    case ProcessPidRole:
        return QVariant(process->pid());
    case Qt::DisplayRole:
    case ProcessNameRole:
        return QVariant(process->name());
    case ProcessIconsRole: {
        QVariantList icons;
        for (QUrl url : process->icons())
            icons.append(url);
        return icons;
    }
    default:
        return QVariant();
    }
}

void ProcessListModel::hardRefresh()
{
    FridaDevice *handle = nullptr;
    if (m_device != nullptr) {
        handle = m_device->handle();
        g_object_ref(handle);
    }

    auto scope = static_cast<FridaScope>(m_scope);

    m_mainContext->schedule([=] () { finishHardRefresh(handle, scope); });

    if (!m_processes.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, m_processes.size() - 1);
        qDeleteAll(m_processes);
        m_processes.clear();
        endRemoveRows();
        Q_EMIT countChanged(0);
    }
}

void ProcessListModel::finishHardRefresh(FridaDevice *handle, FridaScope scope)
{
    m_pids.clear();

    if (handle != nullptr)
        enumerateProcesses(handle, scope);
}

void ProcessListModel::enumerateProcesses(FridaDevice *handle, FridaScope scope)
{
    QMetaObject::invokeMethod(this, "beginLoading", Qt::QueuedConnection);

    if (m_pendingRequest != nullptr)
        m_pendingRequest->model = nullptr;

    auto options = frida_process_query_options_new();
    frida_process_query_options_set_scope(options, scope);

    auto request = g_slice_new(EnumerateProcessesRequest);
    request->model = this;
    request->handle = handle;
    m_pendingRequest = request;

    frida_device_enumerate_processes(handle, options, nullptr, onEnumerateReadyWrapper, request);

    g_object_unref(options);
}

void ProcessListModel::onEnumerateReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data)
{
    Q_UNUSED(obj);

    auto request = static_cast<EnumerateProcessesRequest *>(data);
    if (request->model != nullptr)
        request->model->onEnumerateReady(request->handle, res);
    g_object_unref(request->handle);
    g_slice_free(EnumerateProcessesRequest, request);
}

void ProcessListModel::onEnumerateReady(FridaDevice *handle, GAsyncResult *res)
{
    m_pendingRequest = nullptr;

    QMetaObject::invokeMethod(this, "endLoading", Qt::QueuedConnection);

    GError *error = nullptr;
    auto processHandles = frida_device_enumerate_processes_finish(handle, res, &error);
    if (error == nullptr) {
        QSet<unsigned int> current;
        QList<Process *> added;
        QSet<unsigned int> removed;

        const int size = frida_process_list_size(processHandles);
        for (int i = 0; i != size; i++) {
            auto processHandle = frida_process_list_get(processHandles, i);
            auto pid = frida_process_get_pid(processHandle);
            current.insert(pid);
            if (!m_pids.contains(pid)) {
                auto process = new Process(processHandle);
                process->moveToThread(this->thread());
                added.append(process);
                m_pids.insert(pid);
            }
            g_object_unref(processHandle);
        }

        for (unsigned int pid : std::as_const(m_pids)) {
            if (!current.contains(pid)) {
                removed.insert(pid);
            }
        }

        for (unsigned int pid : std::as_const(removed)) {
            m_pids.remove(pid);
        }

        g_object_unref(processHandles);

        if (!added.isEmpty() || !removed.isEmpty()) {
            g_object_ref(handle);
            QMetaObject::invokeMethod(this, "updateItems", Qt::QueuedConnection,
                Q_ARG(void *, handle),
                Q_ARG(QList<Process *>, added),
                Q_ARG(QSet<unsigned int>, removed));
        }
    } else {
        auto message = QString("Failed to enumerate processes: ").append(QString::fromUtf8(error->message));
        QMetaObject::invokeMethod(this, "onError", Qt::QueuedConnection,
            Q_ARG(QString, message));
        g_clear_error(&error);
    }
}

int ProcessListModel::score(Process *process)
{
    return process->hasIcons() ? 1 : 0;
}

void ProcessListModel::updateItems(void *handle, QList<Process *> added, QSet<unsigned int> removed)
{
    for (Process *process : std::as_const(added)) {
        process->setParent(this);
    }

    g_object_unref(handle);

    if (m_device.isNull() || handle != m_device->handle())
        return;

    int previousCount = m_processes.count();

    QModelIndex parentRow;

    for (unsigned int pid : std::as_const(removed)) {
        auto size = m_processes.size();
        for (int i = 0; i != size; i++) {
            auto process = m_processes[i];
            if (process->pid() == pid) {
                beginRemoveRows(parentRow, i, i);
                m_processes.removeAt(i);
                endRemoveRows();
                delete process;
                break;
            }
        }
    }

    for (Process *process : std::as_const(added)) {
        QString name = process->name();
        auto processScore = score(process);
        int index = -1;
        auto size = m_processes.size();
        for (int i = 0; i != size && index == -1; i++) {
            auto curProcess = m_processes[i];
            auto curProcessScore = score(curProcess);
            if (processScore > curProcessScore) {
                index = i;
            } else if (processScore == curProcessScore) {
                auto nameDifference = name.compare(curProcess->name(), Qt::CaseInsensitive);
                if (nameDifference < 0 || (nameDifference == 0 && process->pid() < curProcess->pid())) {
                    index = i;
                }
            }
        }
        if (index == -1)
            index = size;
        beginInsertRows(parentRow, index, index);
        m_processes.insert(index, process);
        endInsertRows();
    }

    int newCount = m_processes.count();
    if (newCount != previousCount)
        Q_EMIT countChanged(newCount);
}

void ProcessListModel::beginLoading()
{
    m_isLoading = true;
    Q_EMIT isLoadingChanged(m_isLoading);
}

void ProcessListModel::endLoading()
{
    m_isLoading = false;
    Q_EMIT isLoadingChanged(m_isLoading);
}

void ProcessListModel::onError(QString message)
{
    Q_EMIT error(message);
}
