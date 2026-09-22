project('feature user option',
  default_options : ['auto_features=disabled'])

feature_opts = get_option('auto_features')
required_opt = get_option('required')
optional_opt = get_option('optional')
disabled_opt = get_option('disabled')

assert(not feature_opts.enabled(), 'Should be disabled option')
assert(feature_opts.disabled(), 'Should be disabled option')
assert(not feature_opts.auto(), 'Should be disabled option')

assert(required_opt.enabled(), 'Should be enabled option')
assert(not required_opt.disabled(), 'Should be enabled option')
assert(not required_opt.auto(), 'Should be enabled option')

assert(not optional_opt.enabled(), 'Auto feature should be disabled')
assert(optional_opt.disabled(), 'Auto feature should be disabled')
assert(not optional_opt.auto(), 'Auto feature should be disabled')

assert(not disabled_opt.enabled(), 'Should be disabled option')
assert(disabled_opt.disabled(), 'Should be disabled option')
assert(not disabled_opt.auto(), 'Should be disabled option')
