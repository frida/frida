project('install path',
  default_options: [
    'python.bytecompile=-1',
    'python.purelibdir=/pure',
    'python.platlibdir=/plat',
  ]
)

py = import('python').find_installation()
py.install_sources('test.py')
py.install_sources('test.py', pure: false)
install_data('test.py', install_dir: py.get_install_dir() / 'data')
install_data('test.py', install_dir: py.get_install_dir(pure: false) / 'data')

py_plat = import('python').find_installation(pure: false)
py_plat.install_sources('test.py', subdir: 'kw')
py_plat.install_sources('test.py', pure: true, subdir: 'kwrevert')
install_data('test.py', install_dir: py_plat.get_install_dir() / 'kw/data')
install_data('test.py', install_dir: py_plat.get_install_dir(pure: true) / 'kwrevert/data')

if get_option('backend') == 'none'
    subdir('target')
endif

subdir('structured')
