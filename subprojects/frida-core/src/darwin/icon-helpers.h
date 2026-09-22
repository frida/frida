#ifndef __FRIDA_DARWIN_ICON_HELPERS_H__
#define __FRIDA_DARWIN_ICON_HELPERS_H__

#include "frida-core.h"

typedef gpointer FridaNativeImage;

GVariant * _frida_icon_from_file (const gchar * filename, guint target_width, guint target_height);
GVariant * _frida_icon_from_native_image_scaled_to (FridaNativeImage native_image, guint target_width, guint target_height);

#endif
