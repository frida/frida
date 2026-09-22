#include "maincontext.h"

MainContext::MainContext(GMainContext *mainContext) :
    m_handle(mainContext)
{
    g_mutex_init(&m_mutex);
    g_cond_init(&m_cond);
}

MainContext::~MainContext()
{
    g_cond_clear(&m_cond);
    g_mutex_clear(&m_mutex);
}

void MainContext::schedule(std::function<void ()> f)
{
    auto source = g_idle_source_new();
    g_source_set_callback(source, performCallback, new std::function<void ()>(f), destroyCallback);
    g_source_attach(source, m_handle);
    g_source_unref(source);
}

void MainContext::perform(std::function<void ()> f)
{
    volatile bool finished = false;

    auto work = new std::function<void ()>([this, f, &finished] () {
        f();

        g_mutex_lock(&m_mutex);
        finished = true;
        g_cond_signal(&m_cond);
        g_mutex_unlock(&m_mutex);
    });

    auto source = g_idle_source_new();
    g_source_set_callback(source, performCallback, work, destroyCallback);
    g_source_attach(source, m_handle);
    g_source_unref(source);

    g_mutex_lock(&m_mutex);
    while (!finished)
        g_cond_wait(&m_cond, &m_mutex);
    g_mutex_unlock(&m_mutex);
}

gboolean MainContext::performCallback(gpointer data)
{
    auto f = static_cast<std::function<void ()> *>(data);
    (*f)();
    return FALSE;
}

void MainContext::destroyCallback(gpointer data)
{
    auto f = static_cast<std::function<void ()> *>(data);
    delete f;
}
