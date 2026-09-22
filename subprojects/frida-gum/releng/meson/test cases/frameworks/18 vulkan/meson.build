project('vulkan test', 'c')

vulkan_dep = dependency('vulkan', required : false)
if not vulkan_dep.found()
  error('MESON_SKIP_TEST: vulkan not found.')
endif

e = executable('vulkanprog', 'vulkanprog.c', dependencies : vulkan_dep)

test('vulkantest', e)

# Check we can apply a version constraint
dependency('vulkan', version: '>=@0@'.format(vulkan_dep.version()))
