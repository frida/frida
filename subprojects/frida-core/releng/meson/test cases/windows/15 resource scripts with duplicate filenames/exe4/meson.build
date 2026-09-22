dll_res = win.compile_resources(files('src_dll/version.rc'))
shared_library('lib4', 'src_dll/main.c', dll_res)

exe_res = win.compile_resources(files('src_exe/version.rc'))
executable('exe4', 'src_exe/main.c', exe_res)
