project('proj', 'c')

auto_subproj = subproject('sub', required: get_option('use-subproject'))
assert(auto_subproj.found(), 'Subproject should always be buildable and thus found')

auto_dep = dependency('', fallback: ['sub', 'libSub'], required: true)
assert(auto_dep.found() == true, 'Subproject is required and foundable, dependency should be found.')

disabled_subproj = subproject('disabled_sub', required: get_option('disabled-subproject'))
assert(disabled_subproj.found() == false, 'Disabled subproject should be NOT found')

disabled_dep = dependency('', fallback: ['disabled_sub', 'libSub'], required: false)
assert(disabled_dep.found() == false, 'Subproject was disabled, it should never be built.')
nothing = executable('nothing', 'nothing.c', dependencies: [disabled_dep])

subproj_with_missing_dep = subproject('auto_sub_with_missing_dep', required: get_option('auto-sub-with-missing-dep'))
assert(subproj_with_missing_dep.found() == false, 'Subproject with required=auto and missing dependency should be NOT found')
