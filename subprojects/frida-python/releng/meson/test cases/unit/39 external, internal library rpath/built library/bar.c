int foo_system_value (void);
int faa_system_value (void);

int bar_built_value (int in)
{
    return faa_system_value() + foo_system_value() + in;
}
