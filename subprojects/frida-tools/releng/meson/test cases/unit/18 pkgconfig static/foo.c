int power_level (void)
{
#ifdef FOO_STATIC
    return 9001;
#else
    return 8999;
#endif
}
