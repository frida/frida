project('whole archive', 'c')

if meson.backend() == 'xcode' or \
   meson.backend() == 'vs2010' or \
   meson.backend() == 'vs2012' or \
   meson.backend() == 'vs2013'
    error('MESON_SKIP_TEST: whole-archive not supported in Xcode nor pre-VS2015 IDE. Patches welcome.')
endif

add_project_arguments('-I' + meson.source_root(), language : 'c')

# Test 1: link_whole keeps all symbols
# Make static func1
subdir('st_func1')
# Make shared func2 linking whole func1 archive
subdir('sh_func2_linked_func1')
# Link exe with shared library only
subdir('exe')
# Test that both func1 and func2 are accessible from shared library
test('prog', exe)

# Test 2: link_whole can be used instead of source list, see #2180
# Make static func2
subdir('st_func2')
# Link both func1 and func2 into same shared library
# which does not have any sources other than 2 static libraries
subdir('sh_only_link_whole')
# Link exe2 with shared library only
subdir('exe2')
# Test that both func1 and func2 are accessible from shared library
test('prog2', exe2)

# Test 3: link_whole can be used in declare_dependency()
func1_dep = declare_dependency(link_whole : [st_func1])
# Use dependency to link func1 into shared library
subdir('sh_func2_dep_func1')
# Link exe3 with shared library
subdir('exe3')
# Test that both func1 and func2 are accessible from shared library
test('prog3', exe3)

# Test 4: link_whole can be used in transitive declare_dependency()
func1_trans_dep = declare_dependency(dependencies : func1_dep)
# Use transitive dependency to link func1 into shared library
subdir('sh_func2_transdep_func1')
# Link exe4 with shared library
subdir('exe4')
# Test that both func1 and func2 are accessible from shared library
test('prog4', exe4)
