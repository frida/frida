# Tests whether fortran sources files created during configuration are properly
# scanned for dependency information

project('generated', 'fortran',
  default_options : ['default_library=static'])

conf_data = configuration_data()
conf_data.set('ONE', 1)
conf_data.set('TWO', 2)

mod3_f = custom_target(
  'mod3.f',
  input : 'mod3.f90',
  output : 'mod3.f90',
  # We need a platform agnostic way to do a copy a file, using a custom_target
  # and we need to use the @OUTDIR@, not @OUTPUT@ in order to exercise
  # https://github.com/mesonbuild/meson/issues/9258
  command : [
    find_program('python', 'python3'), '-c',
    'import sys, shutil; shutil.copy(sys.argv[1], sys.argv[2])',
    '@INPUT@', '@OUTDIR@',
  ],
)

three = library('mod3', mod3_f)

templates_basenames = ['mod2', 'mod1']
generated_sources = []
foreach template_basename : templates_basenames
  infilename = '@0@.fpp'.format(template_basename)
  outfilename = '@0@.f90'.format(template_basename)
  outfile = configure_file(
      input : infilename, output : outfilename, configuration : conf_data)
  generated_sources += [outfile]
endforeach

sources = ['prog.f90'] + generated_sources
exe = executable('generated', sources, link_with: three)
test('generated', exe)
