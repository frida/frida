#include <frida-core.h>

#include "applicationlistmodel.h"

#include "device.h"
#include "maincontext.h"
#include "application.h"

#include <QMetaMethod>

static const int ApplicationIdentifierRole = Qt::UserRole + 0;
static const int ApplicationNameRole = Qt::UserRole + 1;
static const int ApplicationPidRole = Qt::UserRole + 2;
static const int ApplicationIconsRole = Qt::UserRole + 3;

struct EnumerateApplicationsRequest
{
    ApplicationListModel *model;
    FridaDevice *handle;
};

ApplicationListModel::ApplicationListModel(QObject *parent) :
    QAbstractListModel(parent),
    m_isLoading(false),
    m_scope(Frida::Scope::Minimal),
    m_pendingRequest(nullptr),
    m_mainContext(new MainContext(frida_get_main_context()))
{
}

void ApplicationListModel::dispose()
{
    if (m_pendingRequest != nullptr) {
        m_pendingRequest->model = nullptr;
        m_pendingRequest = nullptr;
    }
}

ApplicationListModel::~ApplicationListModel()
{
    m_mainContext->perform([this] () { dispose(); });
}

Application *ApplicationListModel::get(int index) const
{
    if (index < 0 || index >= m_applications.size())
        return nullptr;

    return m_applications[index];
}

void ApplicationListModel::refresh()
{
    if (m_device.isNull())
        return;

    auto handle = m_device->handle();
    g_object_ref(handle);

    auto scope = static_cast<FridaScope>(m_scope);

    m_mainContext->schedule([this, handle, scope] () { enumerateApplications(handle, scope); });
}

Device *ApplicationListModel::device() const
{
    return m_device;
}

void ApplicationListModel::setDevice(Device *device)
{
    if (device == m_device)
        return;

    m_device = device;
    Q_EMIT deviceChanged(device);

    hardRefresh();
}

void ApplicationListModel::setScope(Frida::Scope scope)
{
    if (scope == m_scope)
        return;

    m_scope = scope;
    Q_EMIT scopeChanged(scope);

    hardRefresh();
}

QHash<int, QByteArray> ApplicationListModel::roleNames() const
{
    QHash<int, QByteArray> r;
    r[Qt::DisplayRole] = "display";
    r[ApplicationIdentifierRole] = "identifier";
    r[ApplicationNameRole] = "name";
    r[ApplicationPidRole] = "pid";
    r[ApplicationIconsRole] = "icons";
    return r;
}

int ApplicationListModel::rowCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);

    return m_applications.size();
}

QVariant ApplicationListModel::data(const QModelIndex &index, int role) const
{
    auto application = m_applications[index.row()];
    switch (role) {
    case ApplicationIdentifierRole:
        return QVariant(application->identifier());
    case Qt::DisplayRole:
    case ApplicationNameRole:
        return QVariant(application->name());
    case ApplicationPidRole:
        return QVariant(application->pid());
    case ApplicationIconsRole: {
        QVariantList icons;
        for (QUrl url : application->icons())
            icons.append(url);
        return icons;
    }
    default:
        return QVariant();
    }
}

void ApplicationListModel::hardRefresh()
{
    FridaDevice *handle = nullptr;
    if (m_device != nullptr) {
        handle = m_device->handle();
        g_object_ref(handle);
    }

    auto scope = static_cast<FridaScope>(m_scope);

    m_mainContext->schedule([=] () { finishHardRefresh(handle, scope); });

    if (!m_applications.isEmpty()) {
        beginRemoveRows(QModelIndex(), 0, m_applications.size() - 1);
        qDeleteAll(m_applications);
        m_applications.clear();
        endRemoveRows();
        Q_EMIT countChanged(0);
    }
}

void ApplicationListModel::finishHardRefresh(FridaDevice *handle, FridaScope scope)
{
    m_identifiers.clear();

    if (handle != nullptr)
        enumerateApplications(handle, scope);
}

void ApplicationListModel::enumerateApplications(FridaDevice *handle, FridaScope scope)
{
    QMetaObject::invokeMethod(this, "beginLoading", Qt::QueuedConnection);

    if (m_pendingRequest != nullptr)
        m_pendingRequest->model = nullptr;

    auto options = frida_application_query_options_new();
    frida_application_query_options_set_scope(options, scope);

    auto request = g_slice_new(EnumerateApplicationsRequest);
    request->model = this;
    request->handle = handle;
    m_pendingRequest = request;
    frida_device_enumerate_applications(handle, options, nullptr, onEnumerateReadyWrapper, request);

    g_object_unref(options);
}

void ApplicationListModel::onEnumerateReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data)
{
    Q_UNUSED(obj);

    auto request = static_cast<EnumerateApplicationsRequest *>(data);
    if (request->model != nullptr)
        request->model->onEnumerateReady(request->handle, res);
    g_object_unref(request->handle);
    g_slice_free(EnumerateApplicationsRequest, request);
}

void ApplicationListModel::onEnumerateReady(FridaDevice *handle, GAsyncResult *res)
{
    m_pendingRequest = nullptr;

    QMetaObject::invokeMethod(this, "endLoading", Qt::QueuedConnection);

    GError *error = nullptr;
    auto applicationHandles = frida_device_enumerate_applications_finish(handle, res, &error);
    if (error == nullptr) {
        QSet<QString> current;
        QList<Application *> added;
        QSet<QString> removed;

        const int size = frida_application_list_size(applicationHandles);
        for (int i = 0; i != size; i++) {
            auto applicationHandle = frida_application_list_get(applicationHandles, i);
            auto identifier = QString::fromUtf8(frida_application_get_identifier(applicationHandle));
            current.insert(identifier);
            if (!m_identifiers.contains(identifier)) {
                auto application = new Application(applicationHandle);
                application->moveToThread(this->thread());
                added.append(application);
                m_identifiers.insert(identifier);
            }
            g_object_unref(applicationHandle);
        }

        for (const QString &identifier : std::as_const(m_identifiers)) {
            if (!current.contains(identifier)) {
                removed.insert(identifier);
            }
        }

        for (const QString &identifier : std::as_const(removed)) {
            m_identifiers.remove(identifier);
        }

        g_object_unref(applicationHandles);

        if (!added.isEmpty() || !removed.isEmpty()) {
            g_object_ref(handle);
            QMetaObject::invokeMethod(this, "updateItems", Qt::QueuedConnection,
                Q_ARG(void *, handle),
                Q_ARG(QList<Application *>, added),
                Q_ARG(QSet<QString>, removed));
        }
    } else {
        auto message = QString("Failed to enumerate applications: ").append(QString::fromUtf8(error->message));
        QMetaObject::invokeMethod(this, "onError", Qt::QueuedConnection,
            Q_ARG(QString, message));
        g_clear_error(&error);
    }
}

int ApplicationListModel::score(Application *application)
{
    return (application->pid() != 0) ? 1 : 0;
}

void ApplicationListModel::updateItems(void *handle, QList<Application *> added, QSet<QString> removed)
{
    for (Application *application : std::as_const(added)) {
        application->setParent(this);
    }

    g_object_unref(handle);

    if (m_device.isNull() || handle != m_device->handle())
        return;

    int previousCount = m_applications.count();

    QModelIndex parentRow;

    for (const QString& identifier : std::as_const(removed)) {
        auto size = m_applications.size();
        for (int i = 0; i != size; i++) {
            auto application = m_applications[i];
            if (application->identifier() == identifier) {
                beginRemoveRows(parentRow, i, i);
                m_applications.removeAt(i);
                endRemoveRows();
                delete application;
                break;
            }
        }
    }

    for (Application *application : std::as_const(added)) {
        QString name = application->name();
        auto applicationScore = score(application);
        int index = -1;
        auto size = m_applications.size();
        for (int i = 0; i != size && index == -1; i++) {
            auto curApplication = m_applications[i];
            auto curApplicationScore = score(curApplication);
            if (applicationScore > curApplicationScore) {
                index = i;
            } else if (applicationScore == curApplicationScore) {
                auto nameDifference = name.compare(curApplication->name(), Qt::CaseInsensitive);
                if (nameDifference < 0 || (nameDifference == 0 && application->pid() < curApplication->pid())) {
                    index = i;
                }
            }
        }
        if (index == -1)
            index = size;
        beginInsertRows(parentRow, index, index);
        m_applications.insert(index, application);
        endInsertRows();
    }

    int newCount = m_applications.count();
    if (newCount != previousCount)
        Q_EMIT countChanged(newCount);
}

void ApplicationListModel::beginLoading()
{
    m_isLoading = true;
    Q_EMIT isLoadingChanged(m_isLoading);
}

void ApplicationListModel::endLoading()
{
    m_isLoading = false;
    Q_EMIT isLoadingChanged(m_isLoading);
}

void ApplicationListModel::onError(QString message)
{
    Q_EMIT error(message);
}
