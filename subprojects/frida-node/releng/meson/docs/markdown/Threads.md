---
short-description: Enabling thread support
...

# Threads

Meson has a very simple notational shorthand for enabling thread
support on your build targets. First you obtain the thread dependency
object like this:

```meson
thread_dep = dependency('threads')
```

And then you just use it in a target like this:

```meson
executable('threadedprogram', ...
  dependencies : thread_dep)
```
