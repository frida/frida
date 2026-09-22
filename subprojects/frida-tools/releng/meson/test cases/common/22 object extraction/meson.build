project('object extraction', 'c')

if meson.is_unity()
  message('Skipping extraction test because this is a Unity build.')
else
  lib1 = library('somelib', 'src/lib.c')
  lib2 = library('somelib2', 'lib.c', 'header.h', 'lib2.c')

  obj1 = lib1.extract_objects('src/lib.c')
  obj2 = lib2.extract_objects(['lib.c'])
  obj3 = lib2.extract_objects(files('lib.c'))
  obj4 = lib2.extract_objects(['lib.c', 'lib.c'])
  obj5 = lib2.extract_objects(['lib.c', 'header.h'])
  obj6 = lib2.extract_all_objects(recursive: true)

  e1 = executable('main1', 'main.c', objects : obj1)
  e2 = executable('main2', 'main.c', objects : obj2)
  e3 = executable('main3', 'main.c', objects : obj3)
  e4 = executable('main4', 'main.c', objects : obj4)
  e5 = executable('main5', 'main.c', objects : obj5)
  e6 = executable('main6', 'main.c', objects : obj6)

  ct_src = custom_target('lib3.c', output: 'lib3.c', capture: true,
                         command: [find_program('create-source.py'), 'lib.c'])
  lib3 = library('somelib3', ct_src)
  e7 = executable('main7', 'main.c', objects: lib3.extract_objects(ct_src[0]))
  e8 = executable('main8', 'main.c', objects: lib3.extract_objects(ct_src))

  gen = generator(find_program('create-source.py'), arguments: ['@INPUT@'],
                  output: '@BASENAME@4.c', capture: true)
  gen_src = gen.process('lib.c')
  lib4 = library('somelib4', gen_src)
  e9 = executable('main9', 'main.c', objects: lib4.extract_objects(gen_src))

  custom_target('custom_target with object inputs', output: 'objs',
                input: [obj1, obj2, obj3, obj5, obj6],
                build_by_default: true,
                command: [find_program('check-obj.py'), meson.backend(), '@INPUT@'],
                capture: true)

  test('extraction test 1', e1)
  test('extraction test 2', e2)
  test('extraction test 3', e3)
  test('extraction test 4', e4)
  test('extraction test 5', e5)
  test('extraction test 6', e6)
  test('extraction test 7', e7)
  test('extraction test 8', e8)
  test('extraction test 9', e9)
endif
