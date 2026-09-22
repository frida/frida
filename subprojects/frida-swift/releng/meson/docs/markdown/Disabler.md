---
short-description: Disabling options
...

# Disabling parts of the build

*This feature is available since version 0.44.0.*

The following is a common fragment found in many projects:

```meson
dep = dependency('foo')

# In some different directory

lib = shared_library('mylib', 'mylib.c',
  dependencies : dep)

# And in a third directory

exe = executable('mytest', 'mytest.c',
  link_with : lib)
test('mytest', exe)
```

This works fine but gets a bit inflexible when you want to make this
part of the build optional. Basically it reduces to adding `if/else`
statements around all target invocations. Meson provides a simpler way
of achieving the same with a disabler object.

A disabler object is created with the `disabler` function:

```meson
d = disabler()
```

The only thing you can do to a disabler object is to ask if it has
been found:

```meson
f = d.found() # returns false
```

Any other statement that uses a disabler object will immediately
return a disabler. For example assuming that `d` contains a disabler
object then

```meson
d2 = some_func(d) # value of d2 will be disabler
d3 = true or d2   # value of d3 will be true because of short-circuiting
d4 = false or d2  # value of d4 will be disabler
if d              # neither branch is evaluated
```

Thus to disable every target that depends on the dependency given
above, you can do something like this:

```meson
if use_foo_feature
  d = dependency('foo')
else
  d = disabler()
endif
```

This concentrates the handling of this option in one place and other
build definition files do not need to be sprinkled with `if`
statements.
