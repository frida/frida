# This test is on its own because it is special.
# To run the test you need the prebuilt object
# file for the given platform.
#
# Combined with cross compilation this would make
# the state space explode so let's just keep this
# in its own subdir so it's not run during cross
# compilation tests.

project('prebuilt object', 'c')

if host_machine.system() == 'windows'
  prebuilt = 'prebuilt.obj'
else
  prebuilt = 'prebuilt.o'
endif

# Remember: do not put source.c in this
# declaration. run_tests.py generates the
# prebuilt object before running this test.

e = []

e += executable('exe1', sources: 'main.c', objects: prebuilt)
e += executable('exe2', sources: 'main.c', objects: files(prebuilt))

sl1 = static_library('lib3', objects: prebuilt)
e += executable('exe3', sources: 'main.c', objects: sl1.extract_all_objects(recursive: true))

ct = custom_target(output: 'copy-' + prebuilt, input: prebuilt,
                   command: [find_program('cp.py'), '@INPUT@', '@OUTPUT@'])
e += executable('exe4', 'main.c', ct)
e += executable('exe5', 'main.c', ct[0])

sl2 = static_library('lib6', sources: ct)
e += executable('exe6', sources: 'main.c', objects: sl2.extract_all_objects(recursive: true))

e += executable('exe7', sources: 'main.c', objects: ct)
e += executable('exe8', sources: 'main.c', objects: ct[0])

sl3 = static_library('lib9', objects: ct)
e += executable('exe9', sources: 'main.c', objects: sl2.extract_all_objects(recursive: true))

foreach i : e
  test(i.name(), i)
endforeach
