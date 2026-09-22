int func1b();
int func2();

int main(int argc, char *argv[])
{
  return func2() + func1b() == 3 ? 0 : 1;
}
