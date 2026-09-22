project('object generator', 'c')

if meson.backend() == 'xcode'
    error('MESON_SKIP_TEST object-only libraries not supported in Xcode. Patches welcome.')
endif

# FIXME: Note that this will not add a dependency to the compiler executable.
# Code will not be rebuilt if it changes.
comp = find_program('obj_generator.py')

if host_machine.system() == 'windows'
  ext = '.obj'
else
  ext = '.o'
endif

cc = meson.get_compiler('c').cmd_array().get(-1)

# Generate an object file with configure_file to mimic prebuilt objects
# provided by the source tree
source1 = configure_file(input : 'source.c',
  output : 'source' + ext,
  command : [comp, cc, files('source.c'),
             join_paths(meson.current_build_dir(), 'source' + ext)])

obj = static_library('obj', objects : source1)

# Generate an object file manually.
gen = generator(comp,
 output : '@BASENAME@' + ext,
 arguments : [cc, '@INPUT@', '@OUTPUT@'])

generated = gen.process(['source2.c'])

shr = shared_library('shr', generated,
  vs_module_defs : 'source2.def')

# Generate an object file with indexed OUTPUT replacement.
gen2 = generator(comp,
 output : '@BASENAME@' + ext,
 arguments : [cc, '@INPUT@', '@OUTPUT0@'])
generated2 = gen2.process(['source3.c'])

stc = static_library('stc', generated2)

subdir('objdir')

e = executable('prog', 'prog.c', link_with : [obj, shr, stc, subdirfilebuilt_obj, subdirfile_obj, subdirstr_obj],
  install : true)

test('objgen', e)
