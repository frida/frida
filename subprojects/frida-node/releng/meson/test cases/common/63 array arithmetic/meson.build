project('array arithmetic')

array1 = ['foo', 'bar']
array2 = ['qux', 'baz']

if array1 + array2 != ['foo', 'bar', 'qux', 'baz']
  error('Array concatenation is broken')
endif
if array2 + array1 != ['qux', 'baz', 'foo', 'bar']
  error('Array concatenation is broken')
endif

if array1 + array1 + array1 != ['foo', 'bar', 'foo', 'bar', 'foo', 'bar']
  error('Many-array concatenation is broken')
endif
