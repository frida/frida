project('sizeof', 'c', 'cpp')

# Test with C
cc = meson.get_compiler('c')

intsize = cc.sizeof('int')
wcharsize = cc.sizeof('wchar_t', prefix : '#include<wchar.h>')

cd = configuration_data()
cd.set('INTSIZE', intsize)
cd.set('WCHARSIZE', wcharsize)
cd.set('CONFIG', 'config.h')
configure_file(input : 'config.h.in', output : 'config.h', configuration : cd)
s = configure_file(input : 'prog.c.in', output : 'prog.c', configuration : cd)

e = executable('prog', s)
test('sizeof test', e)

# Test with C++
cpp = meson.get_compiler('cpp')

intsize = cpp.sizeof('int')
wcharsize = cpp.sizeof('wchar_t', prefix : '#include<wchar.h>')

cdpp = configuration_data()
cdpp.set('INTSIZE', intsize)
cdpp.set('WCHARSIZE', wcharsize)
cdpp.set('CONFIG', 'config.hpp')
configure_file(input : 'config.h.in', output : 'config.hpp', configuration : cdpp)
spp = configure_file(input : 'prog.c.in', output : 'prog.cc', configuration : cdpp)

epp = executable('progpp', spp)
test('sizeof test c++', epp)
