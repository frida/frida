project('set and get')

var1 = 'test1.txt'
var2 = files('test1.txt')[0]

# Use is_disabler for accessing variables
assert(var1 == 'test1.txt')
assert(not is_disabler(var2))

# Ensure that set variables behave correctly
set_variable('var3', 'test2.txt')
set_variable('var4', files('test2.txt')[0])

assert(var3 == 'test2.txt')
assert(not is_disabler(var4))

# Test Equality
assert(var1 == get_variable('var1'))
assert(var2 == get_variable('var2'))

# Test get_variable directly
assert(get_variable('var1') == 'test1.txt')
assert(not is_disabler(get_variable('var2')))
assert(get_variable('var3') == 'test2.txt')
assert(not is_disabler(get_variable('var4')))

# Test get_variable indirectly

var5 = get_variable('var1')
var6 = get_variable('var2')
var7 = get_variable('var3')
var8 = get_variable('var4')
set_variable('var9', get_variable('var7'))
set_variable('var0', get_variable('var8'))

assert(var5 == 'test1.txt')
assert(not is_disabler(var6))
assert(var7 == 'test2.txt')
assert(not is_disabler(var8))
assert(get_variable('var9') == 'test2.txt')
assert(not is_disabler(get_variable('var0')))
assert(not is_disabler(get_variable('var0', var8)))
assert(not is_disabler(get_variable('----', var8)))
assert(not is_disabler(get_variable('----', [var8])))
assert(not is_disabler(get_variable('----', {'asd': var8})))

# test dict get
dict = {'a': var2}

dict_t1 = dict['a']
dict_t2 = dict.get('a')
dict_t3 = dict.get('a', var2)
dict_t4 = dict.get('b', var2)

assert(not is_disabler(dict_t1))
assert(not is_disabler(dict_t2))
assert(not is_disabler(dict_t3))
assert(not is_disabler(dict_t4))

# test lists
list = [var2]

list_t1 = list[0]
list_t2 = list.get(0)
list_t3 = list.get(0, var2)
list_t4 = list.get(1, var2)

assert(not is_disabler(list_t1))
assert(not is_disabler(list_t2))
assert(not is_disabler(list_t3))
assert(not is_disabler(list_t4))
