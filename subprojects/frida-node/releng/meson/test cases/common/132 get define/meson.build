project('get define', 'c', 'cpp')

host_system = host_machine.system()

system_define_map = {
  'linux'     : ['__linux__',     '1'],
  'darwin'    : ['__APPLE__',     '1'],
  'windows'   : ['_WIN32',        '1'],
  'cygwin'    : ['__CYGWIN__',    '1'],
  'haiku'     : ['__HAIKU__',     '1'],
  'dragonfly' : ['__DragonFly__', '1'],
  'netbsd'    : ['__NetBSD__',    '1'],
  'openbsd'   : ['__OpenBSD__',   '1'],
  'gnu'       : ['__GNU__',       '1'],
  'sunos'     : ['__sun__',       '1'],

  # The __FreeBSD__ define will be equal to the major version of the release
  # (ex, in FreeBSD 11.x, __FreeBSD__ == 11). To make the test robust when
  # being run on various versions of FreeBSD, just test that the define is
  # set.
  'freebsd'   : ['__FreeBSD__'],
}

foreach lang : ['c', 'cpp']
  cc = meson.get_compiler(lang)

  if not system_define_map.has_key(host_system)
    error('Please report a bug and help us improve support for this platform')
  endif

  system_define = system_define_map.get(host_system)

  def_name  = system_define[0]
  def_val   = cc.get_define(system_define[0])
  def_exist = cc.has_define(system_define[0])

  assert((def_val != '') == def_exist,
   'The has_define and get_define results for @0@ disagree with each other'.format(def_name))

  if system_define.length() == 2
    assert(def_val == system_define[1],
      '@0@ value is @1@ instead of @2@'.format(def_name, def_val, system_define[1]))
  elif system_define.length() == 1
    assert(def_val != '', '@0@ value is unset'.format(def_name))
  else
    error('Invalid number of items in system_define array, this is a bug in the test!')
  endif

  if cc.find_library('z', required : false).found()
    # When a C file containing #include <foo.h> is pre-processed and foo.h is
    # found in the compiler's default search path, GCC inserts an extra comment
    # between the delimiter and the define which causes a parsing error.
    # https://github.com/mesonbuild/meson/issues/1726
    if host_machine.system() == 'netbsd' or host_machine.system() == 'openbsd'
      # NetBSD and OpenBSD's zlib don't have a ZLIB_VER_MAJOR, but they do have
      # a ZLIB_VERSION (which is a string), so check the first non-quote
      # character of that.
      ver = cc.get_define('ZLIB_VERSION', prefix : '#include <zlib.h>')[1]
      assert(ver == '1', 'ZLIB_VERSION (major) value is "@0@" instead of "1"'.format(ver))
    else
      ver = cc.get_define('ZLIB_VER_MAJOR', prefix : '#include <zlib.h>')
      assert(ver == '1', 'ZLIB_VER_MAJOR value is "@0@" instead of "1"'.format(ver))
    endif
  endif

  # Check that an undefined value is empty.
  have_val = cc.get_define('MESON_FAIL_VALUE')
  have     = cc.has_define('MESON_FAIL_VALUE')
  assert(have_val == '', 'MESON_FAIL_VALUE value is "@0@" instead of ""'.format(have_val))
  assert(not have, 'MESON_FAIL_VALUE was found even though it should not have been')

  # Check that an empty define is reported as existing.
  have_val = cc.get_define('MESON_EMPTY_VALUE', prefix: ['#define MESON_EMPTY_VALUE'])
  have     = cc.has_define('MESON_EMPTY_VALUE', prefix: ['#define MESON_EMPTY_VALUE'])
  assert(have_val == '', 'MESON_EMPTY_VALUE value is "@0@" instead of ""'.format(have_val))
  assert(have, 'MESON_EMPTY_VALUE was not found even though it should have been')

  # Check if prefix array works properly and has the expected order
  have = cc.get_define('MESON_FAIL_VALUE', prefix: ['#define MESON_FAIL_VALUE 1', '#undef MESON_FAIL_VALUE'])
  assert(have == '', 'MESON_FAIL_VALUE value is "@0@" instead of ""'.format(have))

  have = cc.get_define('MESON_SUCCESS_VALUE', prefix: ['#undef MESON_SUCCESS_VALUE', '#define MESON_SUCCESS_VALUE 1'])
  assert(have == '1', 'MESON_SUCCESS_VALUE value is "@0@" instead of ""'.format(have))

  # This is used in the test_preprocessor_checks_CPPFLAGS() unit test.
  have = cc.get_define('MESON_TEST_DEFINE_VALUE')
  expect = get_option('MESON_TEST_DEFINE_VALUE')
  assert(have == expect, 'MESON_TEST_DEFINE_VALUE value is "@0@" instead of "@1@"'.format(have, expect))

  run_1665_test = false
  if meson.is_cross_build()
    lang_arg = meson.get_cross_property(lang + '_args', '')
    if lang_arg == '-DMESON_TEST_ISSUE_1665=1'
      run_1665_test = true
    endif
  endif

  if run_1665_test
    have = cc.get_define('MESON_TEST_ISSUE_1665')
    assert(have == '1', 'MESON_TEST_ISSUE_1665 value is "@0@" instead of "1"'.format(have))
  endif

  have = cc.get_define('TEST_VERSION_STR',
        prefix : '#include <concat.h>', include_directories: include_directories('.'))
  assert(have == '"6.0.0"', 'TEST_VERSION_STR value is "@0@" instead of ""6.0.0""'.format(have))

  concat_examples = {
  'TEST_CONCAT_1': '"abcdef"',
  'TEST_CONCAT_2': '1',
  'TEST_CONCAT_3': '1 2 3',
  'TEST_CONCAT_4': '"ab" 1 "cd"',
  'TEST_CONCAT_5': '"ab\"cd"',
  'TEST_CONCAT_6': '"ab\" \"cd"',
  }
  foreach def,expected : concat_examples
    have = cc.get_define(def,
          prefix : '#include <concat.h>', include_directories: include_directories('.'))
    assert(have == expected, '@0@ value is "@1@" instead of "@2@"'.format(def, have, expected))
  endforeach
endforeach
