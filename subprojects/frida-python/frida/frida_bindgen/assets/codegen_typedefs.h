typedef struct _PyGObject                      PyGObject;
typedef void (* PyGObjectInitFromHandleFunc)  (PyObject * self, gpointer handle);
typedef struct _PyGObjectType                  PyGObjectType;
typedef struct _PyGObjectSignalClosure         PyGObjectSignalClosure;
typedef struct _PyFridaRequest                 PyFridaRequest;
typedef void (* PyFridaCompleteFunc)          (GTask * task, PyObject * value, PyObject * error);
