project('compiler checks with dependencies', 'c')

cc = meson.get_compiler('c')

glib = dependency ('glib-2.0')
if glib.found()
  assert (cc.has_header('glib.h', dependencies : glib), 'glib.h not found')
  assert (cc.has_type('gint32', prefix : '#include <glib.h>', dependencies : glib), 'gint32 not found')
  assert (cc.has_function('g_print', dependencies : glib), 'g_print not found')
  assert (cc.has_member('GError', 'message', prefix : '#include <glib.h>', dependencies : glib), 'GError::message not found')
  assert (cc.has_header_symbol('glib.h', 'gint32', dependencies : glib), 'gint32 symbol not found')
  linkcode = '''#include <glib.h>
int main (void) {
  GError *error = g_error_new_literal (0, 0, NULL);
  return error == NULL;
}
  '''
  assert (cc.links(linkcode, dependencies : glib, name : 'Test link against glib'), 'Linking test against glib failed')
endif

zlib = cc.find_library ('z')
if zlib.found()
  linkcode = '''#include<zlib.h>
int main(void) {
  void *ptr = (void*)(deflate);
  return ptr == 0;
}
'''
  assert (cc.has_function('deflate', prefix : '#include<zlib.h>', dependencies : zlib), 'has_function test failed.')
  assert (cc.links(linkcode, dependencies : zlib, name : 'Test link against zlib'), 'Linking test failed against zlib.')
endif

assert(cc.has_function('pthread_create',
    dependencies : dependency('threads'),
    prefix : '#include <pthread.h>'),
  'Could not detect pthread_create with a thread dependency.')
