# This file should expose all possible meson syntaxes
  #   and ensure the AstInterpreter and RawPrinter are able
  
  # to parse and write a file identical to the original.

  project  (       # project comment 1
    # project comment 2
    'rewrite'  ,  # argument comment
    # project comment 3
        'cpp',
    'c',
    default_options: [
        'unity=on',
        'unity_size=50',  # number of cpp / unity. default is 4...
        'warning_level=2',  # eqv to /W3
        'werror=true',  # treat warnings as errors
        'b_ndebug=if-release',  # disable assert in Release
        'cpp_eh=a',  # /EHa exception handling
        'cpp_std=c++17',
        'cpp_winlibs=' + ','.join([ # array comment
        # in array
          # comment
            'kernel32.lib',
            'user32.lib',
            'gdi32.lib',
            'winspool.lib',
            'comdlg32.lib',
            'advapi32.lib',
            'shell32.lib'
            # before comma comment
            ,
            # after comma comment
            'ole32.lib',
            'oleaut32.lib',
            'uuid.lib',
            'odbc32.lib',
            'odbccp32.lib',
            'Delayimp.lib',  # For delay loaded dll
            'OLDNAMES.lib',
            'dbghelp.lib',
            'psapi.lib',
        ]),
    ],
    meson_version: '>=1.2',
    version: '1.0.0',
 )  # project comment 4

cppcoro_dep = dependency('andreasbuhr-cppcoro-cppcoro')
cppcoro = declare_dependency(
    dependencies: [cppcoro_dep.partial_dependency(
        includes: true,
        link_args: true,
        links: true,
        sources: true,
    )],
    # '/await:strict' allows to use <coroutine> rather than <experimental/coroutine> with C++17.
    # We can remove '/await:strict' once we update to C++20.
    compile_args: ['/await:strict'],
    # includes:true doesn't work for now in partial_dependency()
    # This line could be removed once https://github.com/mesonbuild/meson/pull/10122 is released.
    include_directories: cppcoro_dep.get_variable('includedir1'),
)


if get_option('unicode')  #if comment
#if comment 2
  mfc=cpp_compiler.find_library(get_option('debug')?'mfc140ud':'mfc140u')
       # if comment 3
else#elsecommentnowhitespaces
       # else  comment 1
  mfc = cpp_compiler.find_library( get_option( 'debug' ) ? 'mfc140d' : 'mfc140')
# else comment 2
endif  #endif comment


assert(1 in [1, 2], '''1 should be in [1, 2]''')
assert(3 not in [1, 2], '''3 shouldn't be in [1, 2]''')
assert(not (3 in [1, 2]), '''3 shouldn't be in [1, 2]''')

assert('b' in ['a', 'b'], ''''b' should be in ['a', 'b']''')
assert('c' not in ['a', 'b'], ''''c' shouldn't be in ['a', 'b']''')

assert(exe1 in [exe1, exe2], ''''exe1 should be in [exe1, exe2]''')
assert(exe3 not in [exe1, exe2], ''''exe3 shouldn't be in [exe1, exe2]''')

assert('a' in {'a': 'b'}, '''1 should be in {'a': 'b'}''')
assert('b'not in{'a':'b'}, '''1 should be in {'a': 'b'}''')

assert('a'in'abc')
assert('b'  not   in    'def')


w = 'world'
d = {'a': 1, 'b': 0b10101010, 'c': 'pi', 'd': '''a
b
c''', 'e': f'hello @w@', 'f': f'''triple
  formatted
    string # this is not a comment
      hello @w@
''', 'g': [1, 2, 3], 

    'h' # comment a
    : # comment b
0xDEADBEEF # comment c
, # comment d
'hh': 0xfeedc0de, # lowercase hexa
'hhh': 0XaBcD0123,  # mixed case hexa
'oo': 0O123456,  # upper O octa
'bb': 0B1111,  # upper B binary
'i': {'aa': 11, # this is a comment
  'bb': 22}, # a comment inside a dict
'o': 0o754,
'm': -12,  # minus number
'eq': 1 + 3 - 3 % 4 + -( 7 * 8 ),
}  # end of dict comment

hw = d['e']
one = d['g'][0]
  w  +=  '!'


components = {
  'foo': ['foo.c'],
  'bar': ['bar.c'],
	'baz': ['baz.c'], # this line is indented with a tab!
}

# compute a configuration based on system dependencies, custom logic
conf = configuration_data()
conf.set('USE_FOO', 1)

# Determine the sources to compile
sources_to_compile = []
foreach name, sources : components
  if conf.get('USE_@0@'.format(name.to_upper()), 0) == 1
    sources_to_compile += sources
  endif
endforeach


items = ['a', 'continue', 'b', 'break', 'c']
result = []
foreach i : items
  if i == 'continue'
    continue
  elif i == 'break'
    break
  endif
  result += i
endforeach
# result is ['a', 'b']



if a and b
  # do something
endif
if c or d
  # do something
endif
if not e
  # do something
endif
if not (f or g)
  # do something
endif

single_quote = 'contains a \' character'
string_escapes = '\\\'\a\b\f\n\r\t\v\046\x26\u2D4d\U00002d4d\N{GREEK CAPITAL LETTER DELTA}'
no_string_escapes = '''\\\'\a\b\f\n\r\t\v\046\x26\u2D4d\U00002d4d\N{GREEK CAPITAL LETTER DELTA}'''

# FIXME: is it supposed to work? (cont_eol inside string)
# cont_string = 'blablabla\
# blablabla'

# cont_eol with whitespace and comments after
if a \  # comment in cont 1
    and b \  # comment in cont 2
    or c  # comment in cont 3
  message('ok')
endif

if a \
    or b
  debug('help!')
endif


# End of file comment with no linebreak