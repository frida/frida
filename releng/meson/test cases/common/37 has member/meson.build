project('has member', 'c', 'cpp')

compilers = [meson.get_compiler('c'), meson.get_compiler('cpp')]

foreach cc : compilers
  if not cc.has_member('struct tm', 'tm_sec', prefix : '#include<time.h>')
    error('Did not detect member of "struct tm" that exists: "tm_sec"')
  endif

  if cc.has_member('struct tm', 'tm_nonexistent', prefix : '#include<time.h>')
    error('Not existing member "tm_nonexistent" found.')
  endif

  if not cc.has_members('struct tm', 'tm_sec', 'tm_min', prefix : '#include<time.h>')
    error('Did not detect members of "struct tm" that exist: "tm_sec" "tm_min"')
  endif

  if cc.has_members('struct tm', 'tm_sec', 'tm_nonexistent2', prefix : '#include<time.h>')
    error('Not existing member "tm_nonexistent2" found.')
  endif
endforeach
