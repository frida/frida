#import <Foundation/Foundation.h>

int
main (int argc, char * argv[])
{
  NSURL * url = nil;

  @try
  {
    [NSBundle bundleWithURL:url];
  }
  @catch (id err)
  {
    NSLog (@"No worries, mate!");
  }

  return 0;
}
