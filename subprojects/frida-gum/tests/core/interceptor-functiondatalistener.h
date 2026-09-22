/*
 * Copyright (C) 2008-2019 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 * Copyright (C) 2008 Christian Berentsen <jc.berentsen@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#ifndef __INTERCEPTOR_FUNCTIONDATALISTENER_H__
#define __INTERCEPTOR_FUNCTIONDATALISTENER_H__

#include <glib-object.h>
#include <gum/guminvocationlistener.h>

G_BEGIN_DECLS

#define TEST_TYPE_FUNCTION_DATA_LISTENER \
    (test_function_data_listener_get_type ())
G_DECLARE_FINAL_TYPE (TestFunctionDataListener, test_function_data_listener,
    TEST, FUNCTION_DATA_LISTENER, GObject)

typedef struct _TestFunctionInvocationData TestFunctionInvocationData;
typedef struct _TestFuncThreadState TestFuncThreadState;
typedef struct _TestFuncInvState TestFuncInvState;

struct _TestFuncThreadState
{
  gboolean initialized;
  gchar name[8];
};

struct _TestFuncInvState
{
  gchar arg[16];
};

struct _TestFunctionInvocationData
{
  gpointer function_data;
  TestFuncThreadState thread_data;
  TestFuncInvState invocation_data;
};

struct _TestFunctionDataListener
{
  GObject parent;

  guint on_enter_call_count;
  guint on_leave_call_count;

  guint init_thread_state_count;

  TestFunctionInvocationData last_on_enter_data;
  TestFunctionInvocationData last_on_leave_data;

  GSList * a_threads_seen;
  guint a_thread_index;

  GSList * b_threads_seen;
  guint b_thread_index;
};

void test_function_data_listener_reset (TestFunctionDataListener * self);

G_END_DECLS

#endif
