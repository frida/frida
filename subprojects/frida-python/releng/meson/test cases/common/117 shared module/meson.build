project('shared module', 'c')

c = meson.get_compiler('c')

# Windows UWP doesn't support the ToolHelp API we use in this test to emulate
# runtime symbol resolution.
if host_machine.system() == 'windows'
   if not c.compiles('''
#include <windows.h>
#include <tlhelp32.h>

HANDLE func(void)
{
 return CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
}
''')
     error('MESON_SKIP_TEST Windows UWP does not support this test.')
   endif
endif

dl = c.find_library('dl', required : false)
l = shared_library('runtime', 'runtime.c')
# Do NOT link the module with the runtime library. This
# is a common approach for plugins that are only used
# with dlopen. Any symbols are resolved dynamically
# at runtime.  This requires extra help on Windows, so
# should be avoided unless really necessary.
m = shared_module('mymodule', 'module.c')
e = executable('prog', 'prog.c',
  link_with : l, export_dynamic : true, dependencies : dl)
test('import test', e, args : m)

# Same as above, but module created with build_target()
m2 = build_target('mymodule2', 'module.c', target_type: 'shared_module')
test('import test 2', e, args : m2)

# Shared module that does not export any symbols
shared_module('nosyms', 'nosyms.c',
  override_options: ['werror=false'],
  install : true,
  install_dir : join_paths(get_option('libdir'), 'modules'))
