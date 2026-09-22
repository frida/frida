project('tryrun', 'c', 'cpp')

# Complex to exercise all code paths.
if meson.is_cross_build()
  if meson.can_run_host_binaries()
    compilers = [meson.get_compiler('c', native : false), meson.get_compiler('cpp', native : false)]
  else
    compilers = [meson.get_compiler('c', native : true), meson.get_compiler('cpp', native : true)]
  endif
else
  compilers = [meson.get_compiler('c'), meson.get_compiler('cpp')]
endif

ok_code = '''#include<stdio.h>
int main(void) {
  printf("%s\n", "stdout");
  fprintf(stderr, "%s\n", "stderr");
  return 0;
}
'''

error_code = '''int main(void) {
  return 1;
}
'''

no_compile_code = '''int main(void) {
'''

INPUTS = [
  ['String', ok_code, error_code, no_compile_code],
  ['File', files('ok.c'), files('error.c'), files('no_compile.c')],
]

foreach cc : compilers
  foreach input : INPUTS
    type = input[0]
    ok = cc.run(input[1], name : type + ' should succeed')
    err = cc.run(input[2], name : type + ' should fail')
    noc = cc.run(input[3], name : type + ' does not compile')

    if noc.compiled()
      error(type + ' compilation fail test failed.')
    else
      message(type + ' fail detected properly.')
    endif

    if ok.compiled()
      message(type + ' compilation worked.')
    else
      error(type + ' compilation did not work.')
    endif

    if ok.returncode() == 0
      message(type + ' return code ok.')
    else
      error(type + ' return code fail')
    endif

    if err.returncode() == 1
      message(type + ' bad return code ok.')
    else
      error(type + ' bad return code fail.')
    endif

    if ok.stdout().strip() == 'stdout'
      message(type + ' stdout ok.')
    else
      message(type + ' bad stdout.')
    endif

    if ok.stderr().strip() == 'stderr'
      message(type + ' stderr ok.')
    else
      message(type + ' bad stderr.')
    endif
  endforeach
endforeach
