#include <Python.h>

#ifndef Py_LIMITED_API
#error Py_LIMITED_API must be defined.
#elif Py_LIMITED_API != 0x03070000
#error Wrong value for Py_LIMITED_API
#endif

static struct PyModuleDef limited_module = {
   PyModuleDef_HEAD_INIT,
   "limited_api_test",
   NULL,
   -1,
   NULL
};

PyMODINIT_FUNC PyInit_limited(void) {
    return PyModule_Create(&limited_module);
}
