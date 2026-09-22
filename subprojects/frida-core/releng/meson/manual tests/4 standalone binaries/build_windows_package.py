#!/usr/bin/env python3

import os, urllib.request, shutil, subprocess
from glob import glob

sdl_url = 'http://libsdl.org/release/SDL2-devel-2.0.3-VC.zip'
sdl_filename = 'SDL2-devel-2.0.3-VC.zip'
sdl_dir = 'SDL2-2.0.3'

shutil.rmtree('build', ignore_errors=True)
os.mkdir('build')

if not os.path.exists(sdl_filename):
    response = urllib.request.urlopen(sdl_url, timeout=600.0)
    data = response.read()
    open(sdl_filename, 'wb').write(data)

shutil.unpack_archive(sdl_filename, 'build')

libs = glob(os.path.join('build', sdl_dir, 'lib/x86/*'))
[shutil.copy(x, 'build') for x in libs]

# Sorry for this hack but this needs to work during development
# when Meson is not in path.
subprocess.check_call(['python3', r'..\..\meson.py', 'build',
                       '--backend=ninja', '--buildtype=release'])
subprocess.check_call(['ninja'], cwd='build')
shutil.copy('myapp.iss', 'build')
subprocess.check_call([r'\Program Files\Inno Setup 5\ISCC.exe', 'myapp.iss'],
                      cwd='build')
shutil.copy('build/setup.exe', 'myapp 1.0.exe')
shutil.rmtree('build')
