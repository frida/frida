#include "Marshal.hpp"

#include <msclr/marshal.h>

using namespace System;
using namespace System::Collections::Generic;
using namespace System::IO;
using namespace System::Windows;
using namespace System::Windows::Media;
using namespace System::Windows::Media::Imaging;

namespace Frida
{
  String ^
  Marshal::UTF8CStringToClrString (const char * str)
  {
    wchar_t * strUtf16 = reinterpret_cast<wchar_t *> (g_utf8_to_utf16 (str, -1, NULL, NULL, NULL));
    String ^ result = gcnew String (strUtf16);
    g_free (strUtf16);
    return result;
  }

  char *
  Marshal::ClrStringToUTF8CString (String ^ str)
  {
    msclr::interop::marshal_context ^ context = gcnew msclr::interop::marshal_context ();
    const wchar_t * strUtf16 = context->marshal_as<const wchar_t *> (str);
    gchar * strUtf8 = g_utf16_to_utf8 (reinterpret_cast<const gunichar2 *> (strUtf16), -1, NULL, NULL, NULL);
    delete context;
    return strUtf8;
  }

  gchar **
  Marshal::ClrStringArrayToUTF8CStringVector (array<String ^> ^ arr)
  {
    if (arr == nullptr)
      return NULL;
    gchar ** result = g_new0 (gchar *, arr->Length + 1);
    for (int i = 0; i != arr->Length; i++)
      result[i] = Marshal::ClrStringToUTF8CString (arr[i]);
    return result;
  }

  array<unsigned char> ^
  Marshal::ByteArrayToClrArray (gconstpointer data, gsize size)
  {
    if (data == NULL)
      return nullptr;
    array<unsigned char> ^ result = gcnew array<unsigned char> (size);
    pin_ptr<unsigned char> resultStart = &result[0];
    memcpy (resultStart, data, size);
    return result;
  }

  array<unsigned char> ^
  Marshal::BytesToClrArray (GBytes * bytes)
  {
    if (bytes == NULL)
      return nullptr;
    gsize size;
    gconstpointer data = g_bytes_get_data (bytes, &size);
    return ByteArrayToClrArray (data, size);
  }

  GBytes *
  Marshal::ClrByteArrayToBytes (array<unsigned char> ^ arr)
  {
    if (arr == nullptr)
      return NULL;
    pin_ptr<unsigned char> arrStart = &arr[0];
    return g_bytes_new (arrStart, arr->Length);
  }

  IDictionary<String ^, Object ^> ^
  Marshal::ParametersDictToClrDictionary (GHashTable * dict)
  {
    Dictionary<String ^, Object ^> ^ result = gcnew Dictionary<String ^, Object ^> ();

    GHashTableIter iter;
    g_hash_table_iter_init (&iter, dict);

    gpointer rawKey, rawValue;
    while (g_hash_table_iter_next (&iter, &rawKey, &rawValue))
    {
      const gchar * key = static_cast<const gchar *> (rawKey);
      GVariant * value = static_cast<GVariant *> (rawValue);
      result[UTF8CStringToClrString (key)] = VariantToClrObject (value);
    }

    return result;
  }

  Object ^
  Marshal::VariantToClrObject (GVariant * v)
  {
    if (v == NULL)
      return nullptr;

    if (g_variant_is_of_type (v, G_VARIANT_TYPE_STRING))
      return UTF8CStringToClrString (g_variant_get_string (v, NULL));

    if (g_variant_is_of_type (v, G_VARIANT_TYPE_INT64))
      return gcnew Int64 (g_variant_get_int64 (v));

    if (g_variant_is_of_type (v, G_VARIANT_TYPE_BOOLEAN))
      return gcnew Boolean (g_variant_get_boolean (v));

    if (g_variant_is_of_type (v, G_VARIANT_TYPE ("ay")))
    {
      gsize size;
      gconstpointer data = g_variant_get_fixed_array (v, &size, sizeof (guint8));
      return ByteArrayToClrArray (data, size);
    }

    if (g_variant_is_of_type (v, G_VARIANT_TYPE_VARDICT))
    {
      Dictionary<String ^, Object ^> ^ result = gcnew Dictionary<String ^, Object ^> ();

      GVariantIter iter;
      g_variant_iter_init (&iter, v);

      gchar * key;
      GVariant * value;
      while (g_variant_iter_next (&iter, "{sv}", &key, &value))
      {
        result[UTF8CStringToClrString (key)] = VariantToClrObject (value);
        g_variant_unref (value);
        g_free (key);
      }

      return result;
    }

    if (g_variant_is_of_type (v, G_VARIANT_TYPE_ARRAY))
    {
      List<Object ^> ^ result = gcnew List<Object ^> ();

      GVariantIter iter;
      g_variant_iter_init (&iter, v);

      GVariant * value;
      while ((value = g_variant_iter_next_value (&iter)) != NULL)
      {
        result->Add (VariantToClrObject (value));
        g_variant_unref (value);
      }

      return result->ToArray ();
    }

    return nullptr;
  }

  array<ImageSource ^> ^
  Marshal::IconArrayToClrImageSourceArray (Object ^ icons)
  {
    auto result = gcnew List<ImageSource ^> ();

    auto iconsArray = safe_cast<array<Object ^> ^> (icons);
    for (int i = 0; i != iconsArray->Length; i++)
    {
      ImageSource ^ element = IconToClrImageSource (iconsArray[i]);
      if (element != nullptr)
        result->Add (element);
    }

    return result->ToArray ();
  }

  ImageSource ^
  Marshal::IconToClrImageSource (Object ^ icon)
  {
    if (icon == nullptr)
      return nullptr;

    ImageSource ^ result;

    auto iconDict = safe_cast<IDictionary<String ^, Object ^> ^> (icon);
    auto format = safe_cast<String ^> (iconDict["format"]);
    auto image = safe_cast<array<unsigned char> ^> (iconDict["image"]);
    int imageSize = image->Length;

    if (format == "rgba")
    {
      auto width = safe_cast<gint64> (iconDict["width"]);
      auto height = safe_cast<gint64> (iconDict["height"]);

      const guint rowstride = width * 4;

      pin_ptr<unsigned char> pixelsRgba = &image[0];
      guint8 * pixelsBgra = static_cast<guint8 *> (g_memdup (pixelsRgba, imageSize));
      guint8 * rowStart = pixelsBgra;
      for (gint row = 0; row != height; row++)
      {
        guint32 * pixel = reinterpret_cast<guint32 *> (rowStart);
        for (gint col = 0; col != width; col++)
        {
          *pixel = ((*pixel & 0x000000ff) << 16) |
                   ((*pixel & 0x0000ff00) <<  0) |
                   ((*pixel & 0x00ff0000) >> 16) |
                   ((*pixel & 0xff000000) >>  0);
          pixel++;
        }

        rowStart += rowstride;
      }

      WriteableBitmap ^ bitmap = gcnew WriteableBitmap (width, height, 96, 96, PixelFormats::Pbgra32, nullptr);
      bitmap->WritePixels (Int32Rect (0, 0, width, height), IntPtr (pixelsBgra), imageSize, rowstride, 0, 0);

      g_free (pixelsBgra);

      result = bitmap;
    }
    else if (format == "png")
    {
      BitmapImage ^ bitmap = gcnew BitmapImage ();
      bitmap->StreamSource = gcnew MemoryStream (image);

      result = bitmap;
    }
    else
    {
      result = nullptr;
    }

    return result;
  }

  void
  Marshal::ThrowGErrorIfSet (GError ** error)
  {
    if (*error == NULL)
      return;
    String ^ message = UTF8CStringToClrString ((*error)->message);
    g_clear_error (error);
    throw gcnew Exception (message);
  }
}
