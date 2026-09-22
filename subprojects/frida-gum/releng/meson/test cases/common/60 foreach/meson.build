project('foreach', 'c')

tests = [['test1', 'prog1', 'prog1.c'],
  ['test2', 'prog2', 'prog2.c', 'fallback'],
  ['test3', 'prog3', 'prog3.c', 'urgh']]

assert(tests[0].get(3, 'fallbck') == 'fallbck', 'array #1 fallback did not match')
assert(tests[1].get(3, 'failbk') == 'fallback', 'array #2 value did not match')
assert(tests[2].get(3, 'urgh') == 'urgh', 'array #3 value did not match')

foreach i : tests
  test(i.get(0), executable(i.get(1), i.get(2), install : true))

  # Ensure that changing the tests variable does not
  # affect ongoing iteration in the foreach loop.
  #
  # Being able to do that would make Meson Turing complete and
  # we definitely don't want that.
  tests = ['test4', 'prog4', 'prog4.c']
endforeach

items = ['a', 'continue', 'b', 'break', 'c']
result = []
foreach i : items
  if i == 'continue'
    continue
  elif i == 'break'
    break
  endif
  result += i
endforeach

assert(result == ['a', 'b'], 'Continue or break in foreach failed')

items = []
iter = range(2)
foreach i : iter
  items += i
endforeach
assert(items == [0, 1])
assert(iter[1] == 1)

items = []
foreach i : range(1, 2)
  items += i
endforeach
assert(items == [1])

items = []
foreach i : range(1, 10, 2)
  items += i
endforeach
assert(items == [1, 3, 5, 7, 9])
