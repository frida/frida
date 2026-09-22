project('dolphin option', 'c')

d = disabler()

full_path = d.full_path()
assert(is_disabler(full_path), 'Method call is not a disabler')

d2 = dependency(d)
d3 = (d == d2)
d4 = d + 0
d5 = d2 or true
set_variable('d6', disabler())

has_not_changed = false
if is_disabler(d)
    has_not_changed = true
else
    has_not_changed = true
endif
assert(has_not_changed, 'Disabler has changed.')

assert(is_disabler(d), 'Disabler was not identified correctly.')
assert(is_disabler(d2), 'Function laundered disabler was not identified correctly.')
assert(is_disabler(d3), 'Disabler comparison should yield disabler.')
assert(is_disabler(d4), 'Disabler addition should yield disabler.')
assert(is_disabler(d5), 'Disabler logic op should yield disabler.')
assert(is_disabler(d6), 'set_variable with a disabler should set variable to disabler.')

assert(d, 'Disabler did not cause this to be skipped.')
assert(d2, 'Function laundered disabler did not cause this to be skipped.')
assert(d3, 'Disabler comparison should yield disabler and thus this would not be called.')
assert(d4, 'Disabler addition should yield disabler and thus this would not be called.')
assert(d5, 'Disabler logic op should yield disabler and thus this would not be called.')
assert(d6, 'set_variable with a disabler did not cause this to be skipped.')

number = 0

if d
  number = 1
else
  number = 2
endif

has_not_changed = false
if is_disabler(number)
    has_not_changed = true
else
    has_not_changed = true
endif
assert(has_not_changed, 'Number has changed.')

assert(not is_disabler(number), 'Number should not be a disabler.')
assert(number == 0, 'Plain if handled incorrectly, value should be 0 but is @0@'.format(number))

if d.found()
  number = 1
else
  number = 2
endif

assert(number == 2, 'If found handled incorrectly, value should be 2 but is @0@'.format(number))

dep = dependency('notfounddep', required : false, disabler : true)
app = executable('myapp', 'notfound.c', dependencies : [dep])
assert(is_disabler(app), 'App is not a disabler.')
app = executable('myapp', 'notfound.c', dependencies : [[dep]])
assert(is_disabler(app), 'App is not a disabler.')

cc = meson.get_compiler('c')
dep = cc.find_library('notfounddep', required : false, disabler : true)
app = executable('myapp', 'notfound.c', dependencies : [dep])
assert(is_disabler(app), 'App is not a disabler.')

dep = find_program('donotfindme', required : false, disabler : true)
app = executable('myapp', 'notfound.c', dependencies : [dep])
assert(is_disabler(app), 'App is not a disabler.')

has_not_changed = false
if is_disabler(app)
    has_not_changed = true
else
    has_not_changed = true
endif
assert(has_not_changed, 'App has changed.')

assert(not is_disabler(is_variable('d6')), 'is_variable should not return a disabler')
assert(is_variable('d6'), 'is_variable for a disabler should return true')

if_is_not_disabled = false
if is_variable('d6')
    if_is_not_disabled = true
else
    if_is_not_disabled = true
endif
assert(if_is_not_disabled, 'Disabler in is_variable should not skip blocks')

get_d = get_variable('d6')
assert(is_disabler(get_d), 'get_variable should yield a disabler')

get_fallback_d = get_variable('nonexistent', disabler())
assert(is_disabler(get_fallback_d), 'get_variable fallback should yield a disabler')

var_true = true
get_no_fallback_d = get_variable('var_true', disabler())
assert(not is_disabler(get_no_fallback_d), 'get_variable should not fallback to disabler')
assert(get_no_fallback_d, 'get_variable should yield true')

assert(is_disabler(get_variable(disabler())), 'get_variable should yield a disabler')
assert(is_disabler(get_variable(disabler(), var_true)), 'get_variable should yield a disabler')

if_is_disabled = true
if disabler()
  if_is_disabled = false
else
  if_is_disabled = false
endif
assert(if_is_disabled, 'Disabler in "if condition" must skip both blocks')

if not disabler()
  if_is_disabled = false
else
  if_is_disabled = false
endif
assert(if_is_disabled, 'Disabler in "if not condition" must skip both blocks')

if disabler() == 1
  if_is_disabled = false
else
  if_is_disabled = false
endif
assert(if_is_disabled, 'Disabler in "if a==b" must skip both blocks')

loops = 0
disablers = 0
foreach i : [true, disabler(), true]
  loops += 1
  if is_disabler(i)
    disablers += 1
  endif
endforeach
assert(loops == 3, 'Disabler in foreach array')
assert(disablers == 1, 'Disabler in foreach array')

loops = 0
disablers = 0
foreach k, i : {'a': true, 'b': disabler(), 'c': true}
  loops += 1
  if is_disabler(i)
    disablers += 1
  endif
endforeach
assert(loops == 3, 'Disabler in foreach dict')
assert(disablers == 1, 'Disabler in foreach dict')
