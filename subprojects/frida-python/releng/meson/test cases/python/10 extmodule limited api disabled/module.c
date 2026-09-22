#include <Python.h>

#if defined(Py_LIMITED_API)
#error "Py_LIMITED_API's definition by Meson should have been disabled."
#endif

static struct PyModuleDef my_module = {
   PyModuleDef_HEAD_INIT,
   "my_module",
   NULL,
   -1,
   NULL
};

PyMODINIT_FUNC PyInit_my_module(void) {
    return PyModule_Create(&my_module);
}
