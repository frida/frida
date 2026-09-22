project('multiframework', 'objc')

# In theory only 'AppKit' would be enough but there was a bug
# that causes a build failure when defining two modules. The
# arguments for the latter module overwrote the arguments for
# the first one rather than adding to them.
cocoa_dep = dependency('appleframeworks', modules : ['AppKit', 'Foundation'])

executable('deptester',
  'main.m',
  objc_args : ['-fobjc-arc'],
  dependencies : [cocoa_dep],
)
