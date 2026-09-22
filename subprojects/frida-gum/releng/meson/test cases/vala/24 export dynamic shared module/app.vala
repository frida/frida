const string MODULE_LIB = "libapp_module.so";

delegate int ModuleFunc ();

public int app_func () {
  return 41;
}

int main () {
  Module module;
  void *func;
  unowned ModuleFunc mfunc;

  module = Module.open (MODULE_LIB, ModuleFlags.BIND_LAZY);
  module.symbol ("module_func", out func);
  mfunc = (ModuleFunc) func;

  print ("%d\n", mfunc ());

  return 0;
}
