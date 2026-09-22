gen = files('manygen.py')
py3_bin = import('python3').find_python()

buildtype = get_option('buildtype')
buildtype_args = '-Dfooxxx' # a useless compiler argument
cc = meson.get_compiler('c')
if cc.get_argument_syntax() == 'msvc'
  # We need our manually generated code to use the same CRT as the executable.
  # Taken from compilers.py since build files do not have access to this.
  if buildtype == 'debug'
    buildtype_args = '/MDd'
  elif buildtype == 'debugoptimized'
    buildtype_args = '/MDd'
  elif buildtype == 'release'
    buildtype_args = '/MD'
  endif
  outfiles = ['gen_func.lib', 'gen_func.c', 'gen_func.h', 'gen_func.o']
else
  outfiles = ['gen_func.a', 'gen_func.c', 'gen_func.h', 'gen_func.o']
endif

generated = custom_target('manygen',
  output : outfiles,
  input : ['funcinfo.def'],
  command : [py3_bin, gen[0], '@INPUT@', '@OUTDIR@', buildtype_args, cc.get_argument_syntax(), cc.cmd_array()],
)
