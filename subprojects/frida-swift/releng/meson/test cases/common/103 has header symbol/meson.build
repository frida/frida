project(
  'has header symbol',
  'c', 'cpp',
  default_options : ['cpp_std=c++11'],
)

cc = meson.get_compiler('c')
cpp = meson.get_compiler('cpp')

foreach comp : [cc, cpp]
  assert (comp.has_header_symbol('stdio.h', 'int'), 'base types should always be available')
  assert (comp.has_header_symbol('stdio.h', 'printf'), 'printf function not found')
  assert (comp.has_header_symbol('stdio.h', 'FILE'), 'FILE structure not found')
  assert (comp.has_header_symbol('limits.h', 'INT_MAX'), 'INT_MAX define not found')
  assert (not comp.has_header_symbol('limits.h', 'guint64'), 'guint64 is not defined in limits.h')
  assert (not comp.has_header_symbol('stdlib.h', 'FILE'), 'FILE structure is defined in stdio.h, not stdlib.h')
  assert (not comp.has_header_symbol('stdlol.h', 'printf'), 'stdlol.h shouldn\'t exist')
  assert (not comp.has_header_symbol('stdlol.h', 'int'), 'shouldn\'t be able to find "int" with invalid header')
endforeach

# Glibc requires _GNU_SOURCE for ppoll. Other libcs do not.
if cc.has_function('ppoll') and not cc.has_header_symbol('poll.h', 'ppoll')
  assert (cc.has_header_symbol('poll.h', 'ppoll', prefix : '#define _GNU_SOURCE'), 'ppoll should be accessible with _GNU_SOURCE')
endif

assert (cpp.has_header_symbol('iostream', 'std::iostream'), 'iostream not found in iostream.h')
assert (cpp.has_header_symbol('vector', 'std::vector'), 'vector not found in vector.h')
assert (not cpp.has_header_symbol('limits.h', 'std::iostream'), 'iostream should not be defined in limits.h')

# Cross compilation and boost do not mix.
if not meson.is_cross_build()
  boost = dependency('boost', required : false)
  if boost.found()
    assert (cpp.has_header_symbol('boost/math/quaternion.hpp', 'boost::math::quaternion', dependencies : boost), 'quaternion not found')
  else
    assert (not cpp.has_header_symbol('boost/math/quaternion.hpp', 'boost::math::quaternion',  dependencies : boost), 'quaternion found?!')
  endif
endif
