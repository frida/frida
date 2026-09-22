project('Mixing C and D', 'd', 'c')

ldyn = shared_library('stuff', 'libstuff.c', install : true)
ed = executable('appdc_d', 'app.d', link_with : ldyn, install : true)
test('linktest_cdyn', ed)

lstatic = static_library('stuff', 'libstuff.c', install : true)
es = executable('appdc_s', 'app.d', link_with : lstatic, install : true)
test('linktest_cstatic', es)
