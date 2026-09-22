/* Do not use this as the basis of your own libc.
 * The code is probably suboptimal or wonky, as I
 * had no prior experience with this, but instead
 * just fiddled with the code until it worked.
 */

#include<stdio.h>

#define STDOUT 1
#define SYS_WRITE 4

int simple_print(const char *msg, const long bufsize) {
  int count;
  long total_written = 0;
  while(total_written < bufsize) {
    asm(
        "int $0x80\n\t"
        : "=a"(count)
        : "0"(SYS_WRITE), "b"(STDOUT), "c"(msg+total_written), "d"(bufsize-total_written)
        :);
    if(count == 0) {
      return 1;
    }
    total_written += count;
  }
  return 0;
}

int simple_strlen(const char *str) {
  int len = 0;
  while(str[len] != '\0') {
    len++;
  }
  return len;
}
