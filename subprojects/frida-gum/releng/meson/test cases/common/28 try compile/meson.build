project('try compile', 'c', 'cpp')

code = '''#include<stdio.h>
void func(void) { printf("Something.\n"); }
'''

breakcode = '''#include<nonexisting.h>
void func(void) { printf("This won't work.\n"); }
'''

warncode = '''#warning This is a warning
int main(void) { return 0; }
'''

configure_file(
  input: 'foo.h.in',
  output: 'foo.h',
  configuration: {},
)

header_code = '#include "foo.h"'

foreach lang : ['c', 'cpp']
  compiler = meson.get_compiler(lang)

  assert(not compiler.compiles(header_code, name: 'Should not include . by default'))
  assert(compiler.compiles(header_code, name: 'Should include builddir',
    include_directories: include_directories('.'),
  ))

  if compiler.compiles(code, name : 'code should succeed') == false
    error('Compiler ' + compiler.get_id() + ' is fail.')
  endif

  if compiler.compiles(files('valid.c'), name : 'file should succeed') == false
    error('Compiler ' + compiler.get_id() + ' is fail.')
  endif

  copied = configure_file(input: 'valid.c', output: lang + '-valid-copy.c', copy: true)
  if compiler.compiles(copied, name : 'built file should succeed') == false
    error('Compiler ' + compiler.get_id() + ' is fail.')
  endif

  if compiler.compiles(breakcode, name : 'code should fail')
    error('Compiler ' + compiler.get_id() + ' returned true on broken code.')
  endif

  if compiler.compiles(files('invalid.c'), name : 'file should fail')
    error('Compiler ' + compiler.get_id() + ' returned true on broken code.')
  endif

  # MSVC does not support #warning instruction
  if compiler.get_id() != 'msvc'
    # First check that all tests pass without werror, then check they fail with it.
    foreach with_werror : [false, true]
      expect_success = not with_werror
      assert(compiler.compiles(warncode,
        name: f'code with warning compiles with werror=@with_werror@',
        werror: with_werror) == expect_success)
      assert(compiler.links(warncode,
        name: f'code with warning links with werror=@with_werror@',
        werror: with_werror) == expect_success)
      if meson.can_run_host_binaries()
        assert((compiler.run(warncode,
          name: f'code with warning runs with werror=@with_werror@',
          werror: with_werror).returncode() == 0) == expect_success)
      endif
    endforeach
  endif
endforeach
