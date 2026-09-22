project('subproj found dep not found', 'c')

dependency('somedep', required : false,
           fallback : ['nosubproj', 'dep_name'])

dependency('somedep', required : false,
           fallback : ['failingsubproj', 'dep_name'])

dependency('somenotfounddep', required : false,
           fallback : ['somesubproj', 'dep_name'])

dependency('zlibproxy', required : true,
           fallback : ['somesubproj', 'zlibproxy_dep'])

dependency('somedep', required : false,
           fallback : ['nestedsubproj', 'nestedsubproj_dep'])
