project('compiler argument checking test', 'c')

cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments('-meson-goober-arg-for-testing', checked : 'require'), language : 'c')
