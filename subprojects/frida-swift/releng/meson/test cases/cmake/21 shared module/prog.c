
#include <stdio.h>
#include "module.h"

#if SPECIAL_MAGIC_DEFINE != 42
#error "SPECIAL_MAGIC_DEFINE is not defined"
#endif

int func_from_language_runtime(void);
typedef int (*fptr) (void);

#ifdef _WIN32

#include <windows.h>

static wchar_t*
win32_get_last_error (void)
{
    wchar_t *msg = NULL;

    FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER
                    | FORMAT_MESSAGE_IGNORE_INSERTS
                    | FORMAT_MESSAGE_FROM_SYSTEM,
                    NULL, GetLastError (), 0,
                    (LPWSTR) &msg, 0, NULL);
    return msg;
}

int main(int argc, char **argv)
{
    HINSTANCE handle;
    fptr importedfunc;
    int expected, actual;
    int ret = 1;
    if(argc==0) {};

    handle = LoadLibraryA (argv[1]);
    if (!handle) {
        wchar_t *msg = win32_get_last_error ();
        printf ("Could not open %s: %S\n", argv[1], msg);
        goto nohandle;
    }

    importedfunc = (fptr) GetProcAddress (handle, "func");
    if (importedfunc == NULL) {
        wchar_t *msg = win32_get_last_error ();
        printf ("Could not find 'func': %S\n", msg);
        goto out;
    }

    actual = importedfunc ();
    expected = func_from_language_runtime ();
    if (actual != expected) {
        printf ("Got %i instead of %i\n", actual, expected);
        goto out;
    }

    ret = 0;
out:
    FreeLibrary (handle);
nohandle:
    return ret;
}

#else

#include<dlfcn.h>
#include<assert.h>

int main(int argc, char **argv) {
    void *dl;
    fptr importedfunc;
    int expected, actual;
    char *error;
    int ret = 1;
    if(argc==0) {};

    dlerror();
    dl = dlopen(argv[1], RTLD_LAZY);
    error = dlerror();
    if(error) {
        printf("Could not open %s: %s\n", argv[1], error);
        goto nodl;
    }

    importedfunc = (fptr) dlsym(dl, "func");
    if (importedfunc == NULL) {
        printf ("Could not find 'func'\n");
        goto out;
    }

    assert(importedfunc != func_from_language_runtime);

    actual = (*importedfunc)();
    expected = func_from_language_runtime ();
    if (actual != expected) {
        printf ("Got %i instead of %i\n", actual, expected);
        goto out;
    }

    ret = 0;
out:
    dlclose(dl);
nodl:
    return ret;
}

#endif
