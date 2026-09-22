project('string formatting')

templ = '@0@bar@1@'

assert(templ.format('foo', 'baz') == 'foobarbaz', 'Basic string formatting is broken.')

assert('@0@'.format(1) == '1', 'String number formatting is broken.')

assert('@0@'.format(true) == 'true', 'String boolean formatting is broken.')

templ2 = '@0@'
subs2 = '42'

assert(templ2.format(subs2) == '42', 'String formatting with variables is broken.')

assert('@@0@@ @@1@@'.format(1, 2) == '@1@ @2@', 'String format is recursive.')

long = 'abcde'
prefix = 'abc'
suffix = 'cde'

assert(long[0] == 'a')
assert(long[2] == 'c')

assert(long.replace('b', 'd') == 'adcde')
assert(long.replace('z', 'x') == long)
assert(long.replace(prefix, suffix) == 'cdede')

assert(long.startswith(prefix), 'Prefix.')

assert(not long.startswith(suffix), 'Not prefix.')

assert(long.endswith(suffix), 'Suffix.')

assert(not long.endswith(prefix), 'Not suffix.')

assert(long.contains(prefix), 'Does not contain prefix')

assert(long.contains(suffix), 'Does not contain suffix')

assert(long.contains('bcd'), 'Does not contain middle part')

assert(not long.contains('dc'), 'Broken contains')

assert(long.to_upper() == 'ABCDE', 'Broken to_upper')

assert(long.to_upper().to_lower() == long, 'Broken to_lower')

assert('struct stat.st_foo'.underscorify() == 'struct_stat_st_foo', 'Broken underscorify')

assert('#include <foo/bar.h>'.underscorify() == '_include__foo_bar_h_', 'Broken underscorify')

# case should not change, space should be replaced, numbers are ok too
assert('Do SomeThing 09'.underscorify() == 'Do_SomeThing_09', 'Broken underscorify')

assert('3'.to_int() == 3, 'String int conversion does not work.')

assert(true.to_string() == 'true', 'bool string conversion failed')
assert(false.to_string() == 'false', 'bool string conversion failed')
assert(true.to_string('yes', 'no') == 'yes', 'bool string conversion with args failed')
assert(false.to_string('yes', 'no') == 'no', 'bool string conversion with args failed')
assert('@0@'.format(true) == 'true', 'bool string formatting failed')
assert('@0@'.format(['one', 'two']) == '[\'one\', \'two\']', 'list string formatting failed')

assert(' '.join(['a', 'b', 'c']) == 'a b c', 'join() array broken')
assert(''.join(['a', 'b', 'c']) == 'abc', 'empty join() broken')
assert(' '.join(['a']) == 'a', 'single join broken')
assert(' '.join(['a'], ['b', ['c']], 'd') == 'a b c d', 'varargs join broken')

version_number = '1.2.8'

assert(version_number.version_compare('>=1.2.8'), 'Version_compare gt broken')
assert(not version_number.version_compare('>1.2.8'), 'Version_compare greater broken')
assert(not version_number.version_compare('<1.2.8'), 'Version_compare less broken')
assert(version_number.version_compare('<=1.2.8'), 'Version_compare le broken')
assert(version_number.version_compare('==1.2.8'), 'Version_compare eq broken')
assert(not version_number.version_compare('!=1.2.8'), 'Version_compare neq broken')

assert(version_number.version_compare('<2.0'), 'Version_compare major less broken')
assert(version_number.version_compare('>0.9'), 'Version_compare major greater broken')

assert(' spaces	tabs	'.strip() == 'spaces	tabs', 'Spaces and tabs badly stripped')
assert('''
multiline string	'''.strip() == '''multiline string''', 'Newlines badly stripped')
assert('"1.1.20"'.strip('"') == '1.1.20', '" badly stripped')
assert('"1.1.20"'.strip('".') == '1.1.20', '". badly stripped')
assert('"1.1.20"   '.strip('" ') == '1.1.20', '". badly stripped')

bs_c = '''\c'''
bs_bs_c = '''\\c'''
nl = '''
'''
bs_n = '''\n'''
bs_nl = '''\
'''
bs_bs_n = '''\\n'''
bs_bs_nl = '''\\
'''
bs_bs = '''\\'''
bs = '''\'''

assert('\c' == bs_c, 'Single backslash broken')
assert('\\c' == bs_c, 'Double backslash broken')
assert('\\\c' == bs_bs_c, 'Three backslash broken')
assert('\\\\c' == bs_bs_c, 'Four backslash broken')
assert('\n' == nl, 'Newline escape broken')
assert('\\n' == bs_n, 'Double backslash broken before n')
assert('\\\n' == bs_nl, 'Three backslash broken before n')
assert('\\\\n' == bs_bs_n, 'Four backslash broken before n')
assert('\\\\\n' == bs_bs_nl, 'Five backslash broken before n')
assert('\\\\' == bs_bs, 'Double-backslash broken')
assert('\\' == bs, 'Backslash broken')

mysubstring='foobarbaz'
assert(mysubstring.substring() == 'foobarbaz', 'substring is broken')
assert(mysubstring.substring(0) == 'foobarbaz', 'substring is broken')
assert(mysubstring.substring(1) == 'oobarbaz', 'substring is broken')
assert(mysubstring.substring(-5) == 'arbaz', 'substring is broken')
assert(mysubstring.substring(1, 4) == 'oob', 'substring is broken')
assert(mysubstring.substring(1,-5) == 'oob', 'substring is broken')
assert(mysubstring.substring(1, 0) == '', 'substring is broken')
assert(mysubstring.substring(0, 100) == 'foobarbaz', 'substring is broken')
assert(mysubstring.substring(-1, -5) == '', 'substring is broken')
assert(mysubstring.substring(10, -25) == '', 'substring is broken')
assert(mysubstring.substring(-4, 2) == '', 'substring is broken')
assert(mysubstring.substring(10, 9) == '', 'substring is broken')
assert(mysubstring.substring(8, 10) == 'z', 'substring is broken')

# str.splitlines()
assert('foo\nbar\nbaz'.splitlines() == ['foo', 'bar', 'baz'], 'splitlines is broken')
assert(''.splitlines() == [], 'splitlines with empty string is broken')
assert('foo\rbar\nbaz\n'.splitlines() == ['foo', 'bar', 'baz'], 'splitlines trailing newline is broken')
assert('hello\r\nworld'.splitlines() == ['hello', 'world'])
assert(
  '  leading ws\nand trailing\t'.splitlines() == ['  leading ws', 'and trailing\t'],
  'splitlines leading/trailing whitespace is broken',
)
assert('\n\r\n\r'.splitlines() == ['', '', ''], 'splitlines is broken')
assert('foo'.splitlines() == ['foo'], 'splitlines is broken')
