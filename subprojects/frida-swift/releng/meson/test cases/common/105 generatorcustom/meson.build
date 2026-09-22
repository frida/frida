project('generatorcustom', 'c')

creator = find_program('gen.py')
catter = find_program('catter.py')
gen_resx = find_program('gen-resx.py')

gen = generator(creator,
  output: '@BASENAME@.h',
  arguments : ['@INPUT@', '@OUTPUT@'])

res3 = custom_target('gen-res3',
    output : 'res3.txt',
    command : [gen_resx, '@OUTPUT@', '3'])

res4 = custom_target('gen-res4',
    output : 'res4.txt',
    command : [gen_resx, '@OUTPUT@', '4'])

hs = gen.process('res1.txt', 'res2.txt', res3, res4[0])

allinone = custom_target('alltogether',
    input : hs,
    output : 'alltogether.h',
    command : [catter, '@INPUT@', '@OUTPUT@'])

proggie = executable('proggie', 'main.c', allinone)

test('proggie', proggie)

# specifically testing that cross binaries are run with an exe_wrapper
if meson.can_run_host_binaries()
  gen_tool = executable('generator', 'gen.c', native : false)

  c_gen = generator(
    gen_tool,
    output : '@BASENAME@-cpp.h',
    arguments : ['@INPUT@', '@OUTPUT@']
  )

  hs2 = c_gen.process('res1.txt')

  host_exe = executable('host_test', 'host.c', hs2, native : false)
  test('compiled generator', host_exe)
endif
