static int
PyEndpointParameters_init (PyEndpointParameters * self,
                           PyObject * args,
                           PyObject * kw)
{
  int result = -1;
  static char * keywords[] = { "address", "port", "certificate", "origin", "auth_service", "asset_root", NULL };
  char * address = NULL;
  unsigned short int port = 0;
  PyObject * certificate_value = NULL;
  char * origin = NULL;
  PyObject * auth_service_obj = NULL;
  char * asset_root_value = NULL;
  GTlsCertificate * certificate = NULL;
  FridaAuthenticationService * auth_service = NULL;
  GFile * asset_root = NULL;
  FridaEndpointParameters * handle;

  if (PyGObject_tp_init ((PyObject *) self, args, kw) < 0)
    return -1;

  if (!PyArg_ParseTupleAndKeywords (args, kw, "|esHOesOes", keywords,
        "utf-8", &address,
        &port,
        &certificate_value,
        "utf-8", &origin,
        &auth_service_obj,
        "utf-8", &asset_root_value))
    return -1;

  if (certificate_value != NULL && !PyGObject_unmarshal_certificate (certificate_value, &certificate))
    goto beach;

  if (auth_service_obj != NULL && auth_service_obj != Py_None)
    auth_service = g_object_ref (PY_GOBJECT_HANDLE (auth_service_obj));

  if (asset_root_value != NULL)
    asset_root = g_file_new_for_path (asset_root_value);

  handle = frida_endpoint_parameters_new (address, port, certificate, origin, auth_service, asset_root);

  PyGObject_take_handle ((PyGObject *) self, handle, PYFRIDA_TYPE (EndpointParameters));

  result = 0;

beach:
  g_clear_object (&asset_root);
  g_clear_object (&auth_service);
  g_clear_object (&certificate);

  PyMem_Free (asset_root_value);
  PyMem_Free (origin);
  PyMem_Free (address);

  return result;
}
