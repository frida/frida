project('alignment', 'c', 'cpp')

compilers = [meson.get_compiler('c'), meson.get_compiler('cpp')]

foreach cc : compilers
  # These tests should return the same value on all
  # platforms. If (and when) they don't, fix 'em up.
  if cc.alignment('char') != 1
    error('Alignment of char misdetected.')
  endif

  ptr_size = cc.sizeof('void*')
  dbl_alignment = cc.alignment('double')

  # These tests are not thorough. Doing this properly
  # would take a lot of work because it is strongly
  # platform  and compiler dependent. So just check
  # that they produce something fairly sane.

  if ptr_size == 8 or ptr_size == 4
    message('Size of ptr ok.')
  else
    error('Size of ptr misdetected.')
  endif

  if dbl_alignment == 8 or dbl_alignment == 4
    message('Alignment of double ok.')
  else
    error('Alignment of double misdetected.')
  endif
endforeach
