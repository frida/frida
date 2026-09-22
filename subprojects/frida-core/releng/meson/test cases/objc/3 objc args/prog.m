#import<stdio.h>

int main(void)
{
#ifdef MESON_TEST
  int x = 3;
#endif

  printf("x = %d\n", x);
  return 0;
}
