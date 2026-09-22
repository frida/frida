int static1(void);
int static2(void);

int static2(void)
{
    return 1 + static1();
}
