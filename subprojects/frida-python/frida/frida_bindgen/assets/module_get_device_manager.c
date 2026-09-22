static PyObject *
PyFrida_get_device_manager (PyObject * module,
                            PyObject * args)
{
  static PyObject * manager = NULL;

  if (manager == NULL)
    manager = PyObject_CallFunction ((PyObject *) PYFRIDA_TYPE_OBJECT (DeviceManager), NULL);

  Py_IncRef (manager);

  return manager;
}
