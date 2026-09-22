#ifndef COMMON_H
#define COMMON_H 1

/*
 * target-specific code will print in yellow, common code will print
 * in grey.
 */
#ifdef THE_TARGET
#define ANSI_START "\x1b[33;1m"
#define ANSI_END "\x1b[0m"
#else
#define ANSI_START ""
#define ANSI_END ""
#endif

void some_random_function();
void initialize_target();

struct Board {
    Board *next;
    Board();
    virtual ~Board();
    virtual void say_hello() = 0;
    virtual const char *target() = 0;
};

struct Device {
    Device *next;
    Device();
    virtual ~Device();
    virtual void say_hello() = 0;
};

struct Dependency {
    Dependency *next;
    Dependency();
    virtual ~Dependency();
    virtual void initialize() = 0;
};

#endif
