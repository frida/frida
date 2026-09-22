#include "icon-helpers.h"

#ifdef HAVE_MACOS
# import <AppKit/AppKit.h>
# if __MAC_OS_X_VERSION_MIN_REQUIRED < __MAC_10_12
#  define NSCompositingOperationCopy NSCompositeCopy
# endif
#else
# import <UIKit/UIKit.h>
#endif

GVariant *
_frida_icon_from_file (const gchar * filename, guint target_width, guint target_height)
{
#ifdef HAVE_MACOS
  GVariant * result = NULL;
  NSAutoreleasePool * pool;
  NSImage * image;

  pool = [[NSAutoreleasePool alloc] init];

  image = [[NSImage alloc] initWithContentsOfFile:[NSString stringWithUTF8String:filename]];
  if (image != nil)
  {
    result = _frida_icon_from_native_image_scaled_to (image, target_width, target_height);
    [image release];
  }

  [pool release];

  return result;
#else
  return NULL;
#endif
}

GVariant *
_frida_icon_from_native_image_scaled_to (FridaNativeImage native_image, guint target_width, guint target_height)
{
  GVariant * result;
#ifdef HAVE_MACOS
  NSImage * image = (NSImage *) native_image;
  guint rowstride;
  NSBitmapImageRep * rep;
  NSGraphicsContext * context;
  GVariantBuilder builder;

  rowstride = target_width * 4;

  rep = [[NSBitmapImageRep alloc]
      initWithBitmapDataPlanes:nil
                    pixelsWide:target_width
                    pixelsHigh:target_height
                 bitsPerSample:8
               samplesPerPixel:4
                      hasAlpha:YES
                      isPlanar:NO
                colorSpaceName:NSCalibratedRGBColorSpace
                  bitmapFormat:0
                   bytesPerRow:rowstride
                  bitsPerPixel:32];

  context = [NSGraphicsContext graphicsContextWithBitmapImageRep:rep];

  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:context];
  [image drawInRect:NSMakeRect (0, 0, target_width, target_height)
           fromRect:NSZeroRect
          operation:NSCompositingOperationCopy
           fraction:1.0];
  [context flushGraphics];
  [NSGraphicsContext restoreGraphicsState];

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&builder, "{sv}", "format", g_variant_new_string ("rgba"));
  g_variant_builder_add (&builder, "{sv}", "width", g_variant_new_uint16 (target_width));
  g_variant_builder_add (&builder, "{sv}", "height", g_variant_new_uint16 (target_height));
  g_variant_builder_add (&builder, "{sv}", "image",
      g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, rep.bitmapData, rowstride * target_height, sizeof (guint8)));
  result = g_variant_ref_sink (g_variant_builder_end (&builder));

  [rep release];
#else
  UIImage * image = (UIImage *) native_image;
  guint rowstride;
  CGImageRef cgimage;
  CGSize full, scaled;
  guint pixel_buf_size;
  guint8 * pixel_buf;
  guint32 * pixels;
  guint i;
  CGColorSpaceRef colorspace;
  CGContextRef cgctx;
  CGRect target_rect = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
  GVariantBuilder builder;

  rowstride = target_width * 4;

  cgimage = [image CGImage];

  full.width = CGImageGetWidth (cgimage);
  full.height = CGImageGetHeight (cgimage);

  if (full.height > full.width)
  {
    scaled.width = (CGFloat) full.width * ((CGFloat) target_height / full.height);
    scaled.height = target_height;
  }
  else
  {
    scaled.width = target_width;
    scaled.height = (CGFloat) full.height * ((CGFloat) target_width / full.width);
  }

  pixel_buf_size = rowstride * target_height;
  pixel_buf = g_malloc (pixel_buf_size);

  /*
   * HACK ALERT:
   *
   * CoreGraphics does not yet support non-premultiplied, so we make sure it multiplies with the same pixels as
   * those usually rendered onto by the Frida GUI... ICK!
   */
  pixels = (guint32 *) pixel_buf;
  for (i = 0; i != target_width * target_height; i++)
    pixels[i] = GUINT32_TO_BE (0xf0f0f0ff);

  colorspace = CGColorSpaceCreateDeviceRGB ();
  cgctx = CGBitmapContextCreate (pixel_buf, target_width, target_height, 8, rowstride, colorspace,
      kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast);
  g_assert (cgctx != NULL);

  target_rect.size = scaled;

  CGContextDrawImage (cgctx, target_rect, cgimage);

  g_variant_builder_init (&builder, G_VARIANT_TYPE_VARDICT);
  g_variant_builder_add (&builder, "{sv}", "format", g_variant_new_string ("rgba"));
  g_variant_builder_add (&builder, "{sv}", "width", g_variant_new_uint16 (target_width));
  g_variant_builder_add (&builder, "{sv}", "height", g_variant_new_uint16 (target_height));
  g_variant_builder_add (&builder, "{sv}", "image",
      g_variant_new_fixed_array (G_VARIANT_TYPE_BYTE, CGBitmapContextGetData (cgctx), pixel_buf_size, sizeof (guint8)));
  result = g_variant_ref_sink (g_variant_builder_end (&builder));

  CGContextRelease (cgctx);
  CGColorSpaceRelease (colorspace);
  g_free (pixel_buf);
#endif

  return result;
}
