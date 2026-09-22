project('wayland-test-core-only', 'c')

wl_protocols_dep = dependency('wayland-protocols', required : false)
if not wl_protocols_dep.found()
  error('MESON_SKIP_TEST: wayland-protocols not installed')
endif

wl_mod = import('unstable-wayland')
wl_client_dep = dependency('wayland-client')

xdg_shell_xml = wl_mod.find_protocol('xdg-shell')
xdg_shell = wl_mod.scan_xml(xdg_shell_xml, include_core_only : true)
exe = executable('core', 'core.c', xdg_shell, dependencies : wl_client_dep)
test('core', exe)
