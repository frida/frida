project('D Static Library', 'd')

lstatic = static_library('stuff', 'libstuff.d', install : true)
es = executable('app_s', 'app.d', link_with : lstatic, install : true)
test('linktest_static', es)
