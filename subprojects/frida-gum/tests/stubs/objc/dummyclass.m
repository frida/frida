/*
 * Copyright (C) 2021 Abdelrahman Eid <hot3eed@gmail.com>
 *
 * Licence: wxWindows Library Licence, Version 3.1
 */

#import "dummyclass.h"

#import <Foundation/Foundation.h>

@interface DummyClass : NSObject
- (int) dummyMethod:(int)dummyArg;
@end

@implementation DummyClass
- (int) dummyMethod:(int)dummyArg
{
  int dummy = 0;

  for (int i = 0; i != dummyArg; i++)
  {
    for (int j = i; j != dummyArg; j++)
      dummy++;
  }

  return dummy;
}
@end

void *
dummy_class_get_dummy_method_impl (void)
{
  DummyClass * obj = [[DummyClass alloc] init];
  IMP method = [obj methodForSelector:@selector(dummyMethod:)];
  return (void *) method;
}
