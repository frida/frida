project('custom install script', 'c')

meson.add_install_script('myinstall.py', 'diiba/daaba', 'file.dat', dry_run: true)
meson.add_install_script('myinstall.py', 'this/should', 'also-work.dat')

subdir('src')

meson.add_install_script('myinstall.py', 'dir', afile, '--mode=copy')

data = configuration_data()
data.set10('foo', true)
conf = configure_file(
  configuration : data,
  output : 'conf.txt'
)

meson.add_install_script('myinstall.py', 'dir', conf, '--mode=copy')

t = custom_target(
  'ct',
  command : [find_program('customtarget.py'), '@OUTDIR@'],
  output : ['1.txt', '2.txt'],
)

meson.add_install_script('myinstall.py', 'customtarget', t, '--mode=copy')
meson.add_install_script('myinstall.py', 'customtargetindex', t[0], '--mode=copy')

installer = configure_file(
  input : 'myinstall.py',
  output : 'myinstall_copy.py',
  copy : true,
)

meson.add_install_script(installer, 'otherdir', afile, '--mode=copy')

# This executable links on a library built in src/ directory. On Windows this
# means meson must add src/ into $PATH to find the DLL when running it as
# install script.
myexe = executable('prog', 'prog.c',
  link_with: mylib,
  install : true,
)
if meson.can_run_host_binaries()
  meson.add_install_script(myexe)
endif
