project('string arithmetic', meson_version: '>=0.62.0')

assert('foo' + 'bar' == 'foobar')
assert('foo' + 'bar' + 'baz' == 'foobarbaz')

a = 'a'
b = 'b'
assert(a + b + 'c' == 'abc')

# ------------------------------------------------------------------------------
# format strings:
# ------------------------------------------------------------------------------
sub1 = 'the'
sub2 = ' quick\n'
sub3 = '    brown'
sub4 = '\nfox'
x = f'@sub1@@sub2@@sub3@@sub4@'

assert(x == sub1 + sub2 + sub3 + sub4)
assert(x == 'the quick\n    brown\nfox')

# ------------------------------------------------------------------------------
#  multi-line format strings
# ------------------------------------------------------------------------------
y_actual = f'''This is a multi-line comment with string substitution:
  "@sub1@@sub2@@sub3@@sub4@"

And I can even substitute the entry multiple times!

@sub1@
@sub2@
@sub3@
'''

y_expect = '''This is a multi-line comment with string substitution:
  "the quick
    brown
fox"

And I can even substitute the entry multiple times!

the
 quick

    brown
'''
message('actual=' + y_actual)
message('expect=' + y_expect)
assert(y_actual == y_expect)
