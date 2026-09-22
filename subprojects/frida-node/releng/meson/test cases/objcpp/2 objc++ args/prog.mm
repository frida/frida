#import<stdio.h>

class TestClass
{
};

int main(void)
{
#ifdef MESON_OBJCPP_TEST
int x = 1;
#endif

  printf("x = %x\n", x);

  return 0;
}
