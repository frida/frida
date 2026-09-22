project('python kwarg',
    default_options: [
        'python.bytecompile=-1',
        'python.purelibdir=/pure',
    ]
)

py = import('python')
prog_python = py.find_installation('python3', modules : ['os', 'sys', 're'], pure: true)
assert(prog_python.found() == true, 'python not found when should be')

# In meson 1.2 - 1.3.2, there was a bug when a python installation
# with a different version did not have a module, and we try to install
# something with another python version...
py.find_installation('python3.7', modules: ['notamodule'], required: false)
prog_python.install_sources('a.py')

prog_python = py.find_installation('python3', modules : ['thisbetternotexistmod'], required : false)
assert(prog_python.found() == false, 'python not found but reported as found')
