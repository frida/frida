#ifndef FRIDAQML_MAINCONTEXT_H
#define FRIDAQML_MAINCONTEXT_H

#include <frida-core.h>
#include <functional>

class MainContext
{
public:
    MainContext(GMainContext *mainContext);
    ~MainContext();

    void schedule(std::function<void ()> f);
    void perform(std::function<void ()> f);

    GMainContext *handle() const { return m_handle; }

private:
    static gboolean performCallback(gpointer data);
    static void destroyCallback(gpointer data);

    GMainContext *m_handle;
    GMutex m_mutex;
    GCond m_cond;
};

#endif
