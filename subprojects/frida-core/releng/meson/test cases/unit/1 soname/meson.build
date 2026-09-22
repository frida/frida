project('vertest', 'c')

shared_library('nover', 'versioned.c',
 install : true)

shared_library('verset', 'versioned.c',
 install : true,
 version : '4.5.6')

shared_library('soverset', 'versioned.c',
 install : true,
 soversion : '1.2.3')

shared_library('bothset', 'versioned.c',
 install : true,
 soversion : '1.2.3',
 version : '4.5.6')

shared_library('settosame', 'versioned.c',
  install : true,
  soversion : '7.8.9',
  version : '7.8.9')

shared_module('some_module', 'versioned.c',
  install: true)

module1 = shared_module('linked_module1', 'versioned.c',
  install: true)

module2 = shared_module('linked_module2', 'versioned.c',
  install: true)
module2_dep = declare_dependency(link_with: module2)

executable('main1', 'main.c', link_with: module1)
executable('main2', 'main.c', dependencies: module2_dep)
