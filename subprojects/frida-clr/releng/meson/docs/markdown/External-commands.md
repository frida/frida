---
short-description: Running external commands
...

# External commands

As a part of the software configuration, you may want to get extra
data by running external commands. The basic syntax is the following.

```meson
r = run_command('command', 'arg1', 'arg2', 'arg3', check: true)
output = r.stdout().strip()
errortxt = r.stderr().strip()
```

If `check: true` is given, meson will error out if `command` returns with a
non-zero exit code. Alternatively, you can set `check: false` and get the exit
code with `r.returncode()`.

Since 0.52.0, you can pass the command environment as a dictionary:

```meson
run_command('command', 'arg1', 'arg2', env: {'FOO': 'bar'}, check: true)
```

Since 0.50.0, you can also pass the command [[@env]] object:

```meson
env = environment()
env.set('FOO', 'bar')
run_command('command', 'arg1', 'arg2', env: env)
```

The `run_command` function returns an object that can be queried for
return value and text written to stdout and stderr. The `strip` method
call is used to strip trailing and leading whitespace from strings.
Usually output from command line programs ends in a newline, which is
unwanted in string variables. The first argument can be either a
string or an executable you have detected earlier with `find_program`.

Meson will autodetect scripts with a shebang line and run them with
the executable/interpreter specified in it both on Windows and on
Unixes.

Note that you cannot pass your command line as a single string. That
is, calling `run_command('do_something foo bar')` will not work. You
must either split up the string into separate arguments or pass the
split command as an array. It should also be noted that Meson will not
pass the command to the shell, so any command lines that try to use
things such as environment variables, backticks or pipelines will not
work. If you require shell semantics, write your command into a script
file and call that with `run_command`.
