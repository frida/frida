#include "frida-helper-service-glue.h"

#import <Foundation/Foundation.h>

static volatile BOOL frida_run_loop_running = NO;

void
_frida_start_run_loop (void)
{
  NSRunLoop * loop = [NSRunLoop mainRunLoop];

  frida_run_loop_running = YES;
  while (frida_run_loop_running && [loop runMode:NSDefaultRunLoopMode beforeDate:[NSDate distantFuture]])
    ;
}

void
_frida_stop_run_loop (void)
{
  frida_run_loop_running = NO;
  CFRunLoopStop ([[NSRunLoop mainRunLoop] getCFRunLoop]);
}
