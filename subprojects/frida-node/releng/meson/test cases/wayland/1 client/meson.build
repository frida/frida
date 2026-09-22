project('wayland-test-client', 'c')

wl_protocols_dep = dependency('wayland-protocols', required : false)
if not wl_protocols_dep.found()
  error('MESON_SKIP_TEST: wayland-protocols not installed')
endif

wl_client_dep = dependency('wayland-client')
wl_server_dep = dependency('wayland-server')
wl_mod = import('unstable-wayland')
fs = import('fs')

# Client side only
xdg_shell_xml = wl_mod.find_protocol('xdg-shell')
xdg_shell = wl_mod.scan_xml(xdg_shell_xml)
assert(xdg_shell.length() == 2)
assert(fs.name(xdg_shell[0].full_path()) == 'xdg-shell-protocol.c')
assert(fs.name(xdg_shell[1].full_path()) == 'xdg-shell-client-protocol.h')
exe = executable('client', 'client.c', xdg_shell, dependencies : wl_client_dep)
test('client', exe)

# Server side only
presentation_time_xml = wl_mod.find_protocol('presentation-time')
presentation_time = wl_mod.scan_xml(presentation_time_xml, client : false, server : true)
assert(presentation_time.length() == 2)
assert(fs.name(presentation_time[0].full_path()) == 'presentation-time-protocol.c')
assert(fs.name(presentation_time[1].full_path()) == 'presentation-time-server-protocol.h')
exe = executable('server', 'server.c', presentation_time, dependencies : wl_server_dep)
test('server', exe)

# Both sides
viewporter_xml = wl_mod.find_protocol('viewporter')
viewporter = wl_mod.scan_xml(viewporter_xml, client : true, server : true)
assert(viewporter.length() == 3)
assert(fs.name(viewporter[0].full_path()) == 'viewporter-protocol.c')
assert(fs.name(viewporter[1].full_path()) == 'viewporter-client-protocol.h')
assert(fs.name(viewporter[2].full_path()) == 'viewporter-server-protocol.h')
exe = executable('both', 'both.c', viewporter, dependencies : [wl_client_dep, wl_server_dep])
test('both', exe)

# Local xml
xmls = files('test.xml')
gen = wl_mod.scan_xml(xmls)
assert(gen.length() == 2)
assert(fs.name(gen[0].full_path()) == 'test-protocol.c')
assert(fs.name(gen[1].full_path()) == 'test-client-protocol.h')
exe = executable('local', 'local.c', gen, dependencies : wl_client_dep)
test('local', exe)
