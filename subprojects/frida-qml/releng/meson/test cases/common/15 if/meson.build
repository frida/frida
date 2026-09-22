project('if test', 'c')

var1 = true
set_variable('var2', false)

if var1
  exe = executable('prog', 'prog.c')
endif

if var2
  exe = executable('breakbreakbreak', 'crashing.c')
endif

test('iftest', exe)

if not is_variable('var1')
  error('Is_variable fail.')
endif

if is_variable('nonexisting')
  error('Is_variable fail 2.')
endif

if not get_variable('var1', false)
  error('Get_variable fail.')
endif

if get_variable('nonexisting', false)
  error('Get_variable fail.')
endif


# Now test elseif

t = true
f = false

if true
  message('Ok.')
elif true
  error('Error')
else
  error('Error')
endif

if f
  error('Error.')
elif t
  message('Ok')
else
  error('Error')
endif

if f
  error('Error.')
elif false
  error('Error')
else
  message('Ok')
endif

# Test plain else

var = false

if var
  exe = executable('break', 'break.c')
else
  exe = executable('eprog', 'prog.c')
endif

test('elsetest', exe)
