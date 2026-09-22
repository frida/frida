#include <glib-object.h>

#pragma once

#define BAR_TYPE_BAR (bar_bar_get_type())

G_DECLARE_FINAL_TYPE (BarBar, bar_bar, BAR, BAR, GObject)

int bar_bar_return_success(void);
