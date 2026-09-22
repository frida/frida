project('winmain', 'c')

# MinGW windres has a bug due to which it doesn't parse args with space properly:
# https://github.com/mesonbuild/meson/pull/1346
# https://sourceware.org/bugzilla/show_bug.cgi?id=4933
if ['gcc', 'clang'].contains(meson.get_compiler('c').get_id()) and host_machine.system() == 'windows'
  # Construct build_to_src and skip this test if it has spaces
  # because then the -I flag to windres will also have spaces
  # and we know the test will fail
  src_parts = meson.source_root().split('/')
  build_parts = meson.build_root().split('/')

  # Get the common path (which might just be '/' or 'C:/')
  common = []
  done = false
  count = 0
  if src_parts.length() > build_parts.length()
    parts = build_parts
    other = src_parts
  else
    parts = src_parts
    other = build_parts
  endif
  foreach part : parts
    if not done and part == other.get(count)
      common += [part]
    else
      done = true
    endif
    count += 1
  endforeach

  # Create path components to go down from the build root to the common path
  count = 0
  rel = build_parts
  foreach build : build_parts
    if count < build_parts.length() - common.length()
      rel += ['..']
    endif
    count += 1
  endforeach

  # Create path components to go up from the common path to the build root
  count = 0
  foreach src : src_parts
    if count >= common.length()
      rel += [src]
    endif
    count += 1
  endforeach

  build_to_src = '/'.join(rel)

  if build_to_src.contains(' ')
    message('build_to_src is: ' + build_to_src)
    error('MESON_SKIP_TEST build_to_src has spaces')
  endif
  # Welcome to the end of this conditional.
  # We hope you never have to implement something like this.
endif

subdir('res')

foreach id : [0, 1, 2]
  exe = executable('prog_@0@'.format(id), 'prog.c',
    res[id],
    gui_app : true)

  test('winmain_@0@'.format(id), exe)
endforeach
