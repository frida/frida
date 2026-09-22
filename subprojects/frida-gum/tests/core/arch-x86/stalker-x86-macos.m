/*
 * Copyright (C) 2014 Ole André Vadla Ravnås <oleavr@nowsecure.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#include "stalker-x86-fixture.c"

#import <Cocoa/Cocoa.h>

TESTLIST_BEGIN (stalker_macos)
  TESTENTRY (cocoa_performance)
TESTLIST_END ()

static void configure_app (void);
static void create_and_destroy_window (void);

TESTCASE (cocoa_performance)
{
  GTimer * timer;
  gdouble duration_direct, duration_stalked;

  if (!g_test_slow ())
  {
    g_print ("<skipping, run in slow mode> ");
    return;
  }

  timer = g_timer_new ();

  @autoreleasepool
  {
    const guint repeats = 10;
    guint i;

    configure_app ();
    create_and_destroy_window ();
    create_and_destroy_window ();

    g_timer_reset (timer);
    for (i = 0; i != repeats; i++)
      create_and_destroy_window ();
    duration_direct = g_timer_elapsed (timer, NULL);

    fixture->sink->mask = GUM_NOTHING;

    gum_stalker_set_trust_threshold (fixture->stalker, 0);
    gum_stalker_follow_me (fixture->stalker, fixture->transformer,
        GUM_EVENT_SINK (fixture->sink));

    /* warm-up */
    g_timer_reset (timer);
    create_and_destroy_window ();
    g_timer_elapsed (timer, NULL);

    /* the real deal */
    g_timer_reset (timer);
    for (i = 0; i != repeats; i++)
      create_and_destroy_window ();
    duration_stalked = g_timer_elapsed (timer, NULL);

    gum_stalker_unfollow_me (fixture->stalker);

    g_timer_destroy (timer);
  }

  g_print ("<duration_direct=%f duration_stalked=%f ratio=%f> ",
      duration_direct, duration_stalked, duration_stalked / duration_direct);
}

static void
configure_app (void)
{
  [NSApplication sharedApplication];
  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  id menu = [[NSMenu new] autorelease];
  id menu_item = [[NSMenuItem new] autorelease];
  [menu addItem:menu_item];
  [NSApp setMainMenu:menu];
  id app_menu = [[NSMenu new] autorelease];
  id app_name = [[NSProcessInfo processInfo] processName];
  id quit_title = [@"Quit " stringByAppendingString:app_name];
  id quit_menu_item = [[[NSMenuItem alloc]
      initWithTitle:quit_title
             action:@selector (terminate:)
      keyEquivalent:@"q"] autorelease];
  [app_menu addItem:quit_menu_item];
  [menu_item setSubmenu:app_menu];
}

static void
create_and_destroy_window (void)
{
  id window = [[NSWindow alloc]
      initWithContentRect:NSMakeRect (0, 0, 200, 200)
                styleMask:NSWindowStyleMaskTitled
                  backing:NSBackingStoreBuffered
                    defer:NO];
  [window cascadeTopLeftFromPoint:NSMakePoint (20, 20)];
  [window setTitle:@"Gum"];
  [window makeKeyAndOrderFront:nil];

  [NSApp activateIgnoringOtherApps:YES];

  dispatch_async (dispatch_get_main_queue (), ^{
    [window retain];
    [window close];

    dispatch_async (dispatch_get_main_queue (), ^{
      [NSApp stop:NSApp];

      NSEvent * event = [NSEvent otherEventWithType:NSEventTypeApplicationDefined
                                           location:NSMakePoint (0, 0)
                                      modifierFlags:0
                                          timestamp:0.0
                                       windowNumber:0
                                            context:nil
                                            subtype:0
                                              data1:0
                                              data2:0];
      [NSApp postEvent:event
               atStart:true];
    });
  });

  [NSApp run];

  [window release];
}
