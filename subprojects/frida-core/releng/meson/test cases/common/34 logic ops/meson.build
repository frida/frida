project('logicopts')

t = true
f = false

if (true)
  message('Ok.')
else
  error('Not ok.')
endif

if (false)
  error('Not ok.')
else
  message('Ok.')
endif

if (f)
  error('Not ok.')
else
  message('Ok.')
endif

if (t)
  message('Ok.')
else
  error('Not ok.')
endif

if true and t
  message('Ok.')
else
  error('Not ok.')
endif

if t and false
  error('Not ok.')
else
  message('Ok.')
endif

if f and t
  error('Not ok.')
else
  message('Ok.')
endif

if f or false
  error('Not ok.')
else
  message('Ok.')
endif

if true or f
  message('Ok.')
else
  error('Not ok.')
endif

if t or true
  message('Ok.')
else
  error('Not ok.')
endif

if not true
  error('Negation failed.')
else
  message('Ok.')
endif

if not f
  message('Ok.')
else
  error('Negation failed.')
endif


if f or f or f or f or f or f or f or f or t
  message('Ok.')
else
  error('Chain of ors failed.')
endif

if t and t and t and t and t and t and t and t and f
  error('Chain of ands failed.')
else
  message('Ok.')
endif

if t and t or t
  message('Ok.')
else
  error('Combination of and-or failed.')
endif
