project('threads', 'cpp', 'c',
  default_options : ['cpp_std=c++11'])

cc = meson.get_compiler('c')
if cc.has_function_attribute('unused')
  add_project_arguments('-DHAVE_UNUSED', language: 'c')
endif

threaddep = dependency('threads')

test('cppthreadtest',
  executable('cppthreadprog', 'threadprog.cpp',
    dependencies : threaddep
  )
)

test('cthreadtest',
  executable('cthreadprog', 'threadprog.c',
    dependencies : threaddep
  )
)
