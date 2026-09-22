#ifndef FRIDAQML_APPLICATIONLISTMODEL_H
#define FRIDAQML_APPLICATIONLISTMODEL_H

#include "frida.h"

#include <frida-core.h>
#include <QAbstractListModel>
#include <QQmlEngine>

Q_MOC_INCLUDE("application.h")
Q_MOC_INCLUDE("device.h")
class Application;
class Device;
class MainContext;
struct EnumerateApplicationsRequest;

class ApplicationListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(ApplicationListModel)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(Device *device READ device WRITE setDevice NOTIFY deviceChanged)
    Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
    Q_PROPERTY(Frida::Scope scope READ scope WRITE setScope NOTIFY scopeChanged)
    QML_ELEMENT

public:
    explicit ApplicationListModel(QObject *parent = nullptr);
private:
    void dispose();
public:
    ~ApplicationListModel();

    int count() const { return m_applications.size(); }
    Q_INVOKABLE Application *get(int index) const;
    Q_INVOKABLE void refresh();

    Device *device() const;
    void setDevice(Device *device);
    bool isLoading() const { return m_isLoading; }
    Frida::Scope scope() const { return m_scope; }
    void setScope(Frida::Scope scope);

    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent) const override;
    QVariant data(const QModelIndex &index, int role) const override;

Q_SIGNALS:
    void countChanged(int newCount);
    void deviceChanged(Device *newDevice);
    void isLoadingChanged(bool newIsLoading);
    void scopeChanged(Frida::Scope newScope);
    void error(QString message);

private:
    void hardRefresh();
    void finishHardRefresh(FridaDevice *handle, FridaScope scope);
    void enumerateApplications(FridaDevice *handle, FridaScope scope);
    static void onEnumerateReadyWrapper(GObject *obj, GAsyncResult *res, gpointer data);
    void onEnumerateReady(FridaDevice *handle, GAsyncResult *res);

    static int score(Application *application);

private Q_SLOTS:
    void updateItems(void *handle, QList<Application *> added, QSet<QString> removed);
    void beginLoading();
    void endLoading();
    void onError(QString message);

private:
    QPointer<Device> m_device;
    QList<Application *> m_applications;
    bool m_isLoading;
    Frida::Scope m_scope;

    EnumerateApplicationsRequest *m_pendingRequest;
    QSet<QString> m_identifiers;

    QScopedPointer<MainContext> m_mainContext;
};

#endif
