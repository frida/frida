#import<Foundation/NSString.h>

int main(void) {
  int result;
  NSString *str = [NSString new];
  result = [str length];
  [str release];
  return result;
}
