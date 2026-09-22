project('multiline string', 'c')

x = '''hello again'''
y = '''hello
again'''

if x == y
  error('Things are wrong.')
endif

multieol = '''
'''
singleeol = '\n'

if multieol != singleeol
  error('Newline quoting is broken.')
endif

# And one more for good measure.
quote1 = ''' ' '''.strip()
quote2 = '\''

if quote1 != quote2
  error('Single quote quoting is broken.')
endif

cc = meson.get_compiler('c')
prog = '''
#include <stdio.h>

int main(void) {
  int num = 1;
  printf("%d\n", num);
  return 0;
}'''

assert(cc.compiles(prog), 'multiline test compile failed')
