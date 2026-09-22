---
short-description: Syntax and structure of Meson files
...

# Syntax

The syntax of Meson's specification language has been kept as simple
as possible. It is *strongly typed* so no object is ever converted to
another under the covers. Variables have no visible type which makes
Meson *dynamically typed* (also known as *duck typed*).

The main building blocks of the language are *variables*, *numbers*,
*booleans*, *strings*, *arrays*, *function calls*, *method calls*, *if
statements* and *includes*.

Usually one Meson statement takes just one line. There is no way to
have multiple statements on one line as in e.g. *C*. Function and
method calls' argument lists can be split over multiple lines. Meson
will autodetect this case and do the right thing.

In other cases, *(added 0.50)* you can get multi-line statements by
ending the line with a `\`. Apart from line ending whitespace has no
syntactic meaning.

## Variables

Variables in Meson work just like in other high level programming
languages. A variable can contain a value of any type, such as an
integer or a string. Variables don't need to be predeclared, you can
just assign to them and they appear. Here's how you would assign
values to two different variables.

```meson
var1 = 'hello'
var2 = 102
```

One important difference in how variables work in Meson is that all
objects are immutable. When you see an operation which appears like
a mutation, actually a new object is created and assigned to the
name. This is different from, for example, how Python works for
objects, but similar to e.g. Python strings.

```meson
var1 = [1, 2, 3]
var2 = var1
var2 += [4]
# var2 is now [1, 2, 3, 4]
# var1 is still [1, 2, 3]
```

## Numbers

Meson supports only integer numbers. They are declared simply by
writing them out. Basic arithmetic operations are supported.

```meson
x = 1 + 2
y = 3 * 4
d = 5 % 3 # Yields 2.
```

Hexadecimal literals are supported since version 0.45.0:

```meson
int_255 = 0xFF
```

Octal and binary literals are supported since version 0.47.0:

```meson
int_493 = 0o755
int_1365 = 0b10101010101
```

Strings can be converted to a number like this:

```meson
string_var = '42'
num = string_var.to_int()
```

Numbers can be converted to a string:

```meson
int_var = 42
string_var = int_var.to_string()
```

## Booleans

A boolean is either `true` or `false`.

```meson
truth = true
```

Booleans can be converted to a string or to a number:

```meson
bool_var = true
string_var = bool_var.to_string()
int_var = bool_var.to_int()
```

## Strings

Strings in Meson are declared with single quotes. To enter a literal
single quote do it like this:

```meson
single_quote = 'contains a \' character'
```

The full list of escape sequences is:

* `\\` Backslash
* `\'` Single quote
* `\a` Bell
* `\b` Backspace
* `\f` Formfeed
* `\n` Newline
* `\r` Carriage Return
* `\t` Horizontal Tab
* `\v` Vertical Tab
* `\ooo` Character with octal value ooo
* `\xhh` Character with hex value hh
* `\uxxxx` Character with 16-bit hex value xxxx
* `\Uxxxxxxxx` Character with 32-bit hex value xxxxxxxx
* `\N{name}` Character named name in Unicode database

As in python and C, up to three octal digits are accepted in `\ooo`.

Unrecognized escape sequences are left in the string unchanged, i.e., the
backslash is left in the string.

### String concatenation

Strings can be concatenated to form a new string using the `+` symbol.

```meson
str1 = 'abc'
str2 = 'xyz'
combined = str1 + '_' + str2 # combined is now abc_xyz
```

### String path building

*(Added 0.49)*

You can concatenate any two strings using `/` as an operator to build paths.
This will always use `/` as the path separator on all platforms.

```meson
joined = '/usr/share' / 'projectname'    # => /usr/share/projectname
joined = '/usr/local' / '/etc/name'      # => /etc/name

joined = 'C:\\foo\\bar' / 'builddir'     # => C:/foo/bar/builddir
joined = 'C:\\foo\\bar' / 'D:\\builddir' # => D:/builddir
```

Note that this is equivalent to using [[join_paths]],
which was obsoleted by this operator.

### Strings running over multiple lines

Strings running over multiple lines can be declared with three single
quotes, like this:

```meson
multiline_string = '''#include <foo.h>
int main (int argc, char ** argv) {
  return FOO_SUCCESS;
}'''
```

These are raw strings that do not support the escape sequences listed
above.  These strings can also be combined with the string formatting
functionality via `.format()` described below.

Note that multiline f-string support was added in version 0.63.

### String index

Strings support the indexing (`[<num>]`) operator. This operator allows (read
only) accessing a specific character. The returned value is guaranteed to be
a string of length 1.

```meson
foo = 'abcd'
message(foo[1])  # Will print 'b'
foo[2] = 'C'     # ERROR: Meson objects are immutable!
```

### String formatting

#### .format()

Strings can be built using the string formatting functionality.

```meson
template = 'string: @0@, number: @1@, bool: @2@'
res = template.format('text', 1, true)
# res now has value 'string: text, number: 1, bool: true'
```

As can be seen, the formatting works by replacing placeholders of type
`@number@` with the corresponding argument.

#### Format strings
*(Added 0.58)*

Format strings can be used as a non-positional alternative to the
string formatting functionality described above. Note that multiline f-string
support was added in version 0.63.

```meson
n = 10
m = 'hi'

s = f'int: @n@, string: @m@'
# s now has the value 'int: 10, string: hi'
```

Currently only identity-expressions are supported inside of format
strings, meaning you cannot use arbitrary Meson expressions inside of them.

```meson
n = 10
m = 5

# The following is not a valid format string
s = f'result: @n + m@'
```

### String methods

Strings also support a number of other methods that return transformed
copies.

#### .replace()

Since 0.58.0, you can replace a substring from a string.

```meson
# Replaces all instances of one substring with another
s = 'semicolons;as;separators'
s = s.replace('as', 'are')
# 's' now has the value of 'semicolons;are;separators'
```

#### .strip()

```meson
# Similar to the Python str.strip(). Removes leading/ending spaces and newlines.
define = ' -Dsomedefine '
stripped_define = define.strip()
# 'stripped_define' now has the value '-Dsomedefine'

# You may also pass a string to strip, which specifies the set of characters to
# be removed instead of the default whitespace.
string = 'xyxHelloxyx'.strip('xy')
# 'string' now has the value 'Hello'
```

Since 0.43.0, you can specify one positional string argument,
and all characters in that string will be stripped.

#### .to_upper(), .to_lower()

```meson
target = 'x86_FreeBSD'
upper = target.to_upper() # t now has the value 'X86_FREEBSD'
lower = target.to_lower() # t now has the value 'x86_freebsd'
```

#### .to_int()

```meson
version = '1'
# Converts the string to an int and throws an error if it can't be
ver_int = version.to_int()
```

#### .contains(), .startswith(), .endswith()

```meson
target = 'x86_FreeBSD'
is_fbsd = target.to_lower().contains('freebsd')
# is_fbsd now has the boolean value 'true'
is_x86 = target.startswith('x86') # boolean value 'true'
is_bsd = target.to_lower().endswith('bsd') # boolean value 'true'
```

#### .substring()

Since 0.56.0, you can extract a substring from a string.

```meson
# Similar to the Python str[start:end] syntax
target = 'x86_FreeBSD'
platform = target.substring(0, 3) # prefix string value 'x86'
system = target.substring(4) # suffix string value 'FreeBSD'
```

The method accepts negative values where negative `start` is relative to the end of
string `len(string) - start` as well as negative `end`.

```meson
string = 'foobar'
string.substring(-5, -3) # => 'oo'
string.substring(1, -1) # => 'ooba'
```

#### .split(), .join()

```meson
# Similar to the Python str.split()
components = 'a b   c d '.split()
# components now has the value ['a', 'b', 'c', 'd']
components = 'a b   c d '.split(' ')
# components now has the value ['a', 'b', '', '', 'c', 'd', '']

# Similar to the Python str.join()
output = ' '.join(['foo', 'bar'])
# Output value is 'foo bar'
pathsep = ':'
path = pathsep.join(['/usr/bin', '/bin', '/usr/local/bin'])
# path now has the value '/usr/bin:/bin:/usr/local/bin'

# For joining path elements, you should use path1 / path2
# This has the advantage of being cross-platform
path = '/usr' / 'local' / 'bin'
# path now has the value '/usr/local/bin'

# For sources files, use files():
my_sources = files('foo.c')
...
my_sources += files('bar.c')
# This has the advantage of always calculating the correct relative path, even
# if you add files in another directory or use them in a different directory
# than they're defined in

# Example to set an API version for use in library(), install_header(), etc
project('project', 'c', version: '0.2.3')
version_array = meson.project_version().split('.')
# version_array now has the value ['0', '2', '3']
api_version = '.'.join([version_array[0], version_array[1]])
# api_version now has the value '0.2'

# We can do the same with .format() too:
api_version = '@0@.@1@'.format(version_array[0], version_array[1])
# api_version now (again) has the value '0.2'
```

#### .underscorify()

```meson
name = 'Meson Docs.txt#Reference-manual'
# Replaces all characters other than `a-zA-Z0-9` with `_` (underscore)
# Useful for substituting into #defines, filenames, etc.
underscored = name.underscorify()
# underscored now has the value 'Meson_Docs_txt_Reference_manual'
```

#### .version_compare()

```meson
version = '1.2.3'
# Compare version numbers semantically
is_new = version.version_compare('>=2.0')
# is_new now has the boolean value false
# Supports the following operators: '>', '<', '>=', '<=', '!=', '==', '='
```

Meson version comparison conventions include:

```meson
'3.6'.version_compare('>=3.6.0') == false
```

It is best to be unambiguous and specify the full revision level to compare.

## Arrays

Arrays are delimited by brackets. An array can contain an arbitrary number of objects of any type.

```meson
my_array = [1, 2, 'string', some_obj]
```

Accessing elements of an array can be done via array indexing:

```meson
my_array = [1, 2, 'string', some_obj]
second_element = my_array[1]
last_element = my_array[-1]
```

You can add more items to an array like this:

```meson
my_array += ['foo', 3, 4, another_obj]
```

When adding a single item, you do not need to enclose it in an array:

```meson
my_array += ['something']
# This also works
my_array += 'else'
```

Note appending to an array will always create a new array object and
assign it to `my_array` instead of modifying the original since all
objects in Meson are immutable.

Since 0.49.0, you can check if an array contains an element like this:

```meson
my_array = [1, 2]
if 1 in my_array
# This condition is true
endif
if 1 not in my_array
# This condition is false
endif
```

### Array methods

The following methods are defined for all arrays:

 - `length`, the size of the array
 - `contains`, returns `true` if the array contains the object given as argument, `false` otherwise
 - `get`, returns the object at the given index, negative indices count from the back of the array, indexing out of bounds is a fatal error. Provided for backwards-compatibility, it is identical to array indexing.

## Dictionaries

Dictionaries are delimited by curly braces. A dictionary can contain
an arbitrary number of key: value pairs. Keys are required to be
strings, but values can be objects of any type. Prior to *0.53.0* keys
were required to be literal strings, i.e., you could not use a
variable containing a string value as a key.

```meson
my_dict = {'foo': 42, 'bar': 'baz'}
```

Keys must be unique:

```meson
# This will fail
my_dict = {'foo': 42, 'foo': 43}
```

Accessing elements of a dictionary works similarly to array indexing:

```meson
my_dict = {'foo': 42, 'bar': 'baz'}
forty_two = my_dict['foo']
# This will fail
my_dict['does_not_exist']
```

Dictionaries are immutable and do not have a guaranteed order.

Dictionaries are available since 0.47.0.

Visit the [[@dict]] objects page in the Reference Manual to read
about the methods exposed by dictionaries.

Since 0.49.0, you can check if a dictionary contains a key like this:

```meson
my_dict = {'foo': 42, 'bar': 43}
if 'foo' in my_dict
# This condition is true
endif
if 42 in my_dict
# This condition is false
endif
if 'foo' not in my_dict
# This condition is false
endif
```

*Since 0.53.0* Keys can be any expression evaluating to a string
value, not limited to string literals any more.

```meson
d = {'a' + 'b' : 42}
k = 'cd'
d += {k : 43}
```

## Function calls

Meson provides a set of usable functions. The most common use case is
creating build objects.

```meson
executable('progname', 'prog.c')
```

Most functions take only few positional arguments but several keyword
arguments, which are specified like this:

```meson
executable('progname',
  sources: 'prog.c',
  c_args: '-DFOO=1')
```

Starting with version 0.49.0 keyword arguments can be specified
dynamically. This is done by passing dictionary representing the
keywords to set in the `kwargs` keyword. The previous example would be
specified like this:

```meson
d = {'sources': 'prog.c',
  'c_args': '-DFOO=1'}

executable('progname',
  kwargs: d)
```

A single function can take keyword arguments both directly in the
function call and indirectly via the `kwargs` keyword argument. The
only limitation is that it is a hard error to pass any particular key
both as a direct and indirect argument.

```meson
d = {'c_args': '-DFOO'}
executable('progname', 'prog.c',
  c_args: '-DBAZ=1',
  kwargs: d) # This is an error!
```

Attempting to do this causes Meson to immediately exit with an error.

### Argument flattening

Argument flattening is a Meson feature that aims to simplify using
methods and functions. For functions where this feature is active,
Meson takes the list of arguments and flattens all nested lists into
one big list.

For instance the following function calls to [[executable]] are
identical in Meson:

```meson
# A normal example:
executable('exe1', ['foo.c', 'bar.c', 'foobar.c'])

# A more contrived example that also works but certainly
# isn't good Meson code:
l1 = ['bar.c']
executable('exe1', [[['foo.c', l1]], ['foobar.c']])

# How meson will treat all the previous calls internally:
executable('exe1', 'foo.c', 'bar.c', 'foobar.c')
```

Because of an internal implementation detail, the following syntax
is currently also supported, even though the first argument of
[[executable]] is a single [[@str]] and not a [[@list]]:

```meson
# WARNING: This example is only valid because of an internal
#          implementation detail and not because it is intended
#
#          PLEASE DO NOT DO SOMETHING LIKE THIS!
#
executable(['exe1', 'foo.c'], 'bar.c', 'foobar.c')
```

This code is currently accepted because argument flattening *currently*
happens before the parameters are evaluated. "Support" for
such constructs will likely be removed in future Meson releases!

Argument flattening is supported by *most* but not *all* Meson
functions and methods. As a general rule, it can be assumed that a
function or method supports argument flattening if the exact list
structure is irrelevant to a function.

Whether a function supports argument flattening is documented in the
[Reference Manual](Reference-manual.md).

## Method calls

Objects can have methods, which are called with the dot operator. The
exact methods it provides depends on the object.

```meson
myobj = some_function()
myobj.do_something('now')
```

## If statements

If statements work just like in other languages.

```meson
var1 = 1
var2 = 2
if var1 == var2 # Evaluates to false
  something_broke()
elif var3 == var2
  something_else_broke()
else
  everything_ok()
endif

opt = get_option('someoption')
if opt != 'foo'
  do_something()
endif
```

## Logical operations

Meson has the standard range of logical operations which can be used in
`if` statements.

```meson
if a and b
  # do something
endif
if c or d
  # do something
endif
if not e
  # do something
endif
if not (f or g)
  # do something
endif
```

Logical operations work only on boolean values.

## Foreach statements

To do an operation on all elements of an iterable, use the `foreach`
command.

> Note that Meson variables are immutable. Trying to assign a new value
> to the iterated object inside a foreach loop will not affect foreach's
> control flow.

### Foreach with an array

Here's an example of how you could define two executables
with corresponding tests using arrays and foreach.

```meson
progs = [['prog1', ['prog1.c', 'foo.c']],
         ['prog2', ['prog2.c', 'bar.c']]]

foreach p : progs
  exe = executable(p[0], p[1])
  test(p[0], exe)
endforeach
```

### Foreach with a dictionary

Here's an example of you could iterate a set of components that
should be compiled in according to some configuration. This uses
a [dictionary][dictionaries], which is available since 0.47.0.

```meson
components = {
  'foo': ['foo.c'],
  'bar': ['bar.c'],
  'baz': ['baz.c'],
}

# compute a configuration based on system dependencies, custom logic
conf = configuration_data()
conf.set('USE_FOO', 1)

# Determine the sources to compile
sources_to_compile = []
foreach name, sources : components
  if conf.get('USE_@0@'.format(name.to_upper()), 0) == 1
    sources_to_compile += sources
  endif
endforeach
```

### Foreach `break` and `continue`

Since 0.49.0 `break` and `continue` keywords can be used inside foreach loops.

```meson
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
# result is ['a', 'b']
```

## Comments

A comment starts with the `#` character and extends until the end of the line.

```meson
some_function() # This is a comment
some_other_function()
```

## Ternary operator

The ternary operator works just like in other languages.

```meson
x = condition ? true_value : false_value
```

The only exception is that nested ternary operators are forbidden to
improve legibility. If your branching needs are more complex than this
you need to write an `if/else` construct.

## Includes

Most source trees have multiple subdirectories to process. These can
be handled by Meson's `subdir` command. It changes to the given
subdirectory and executes the contents of `meson.build` in that
subdirectory. All state (variables etc) are passed to and from the
subdirectory. The effect is roughly the same as if the contents of the
subdirectory's Meson file would have been written where the include
command is.

```meson
test_data_dir = 'data'
subdir('tests')
```

## User-defined functions and methods

Meson does not currently support user-defined functions or methods.
The addition of user-defined functions would make Meson
Turing-complete which would make it harder to reason about and more
difficult to integrate with tools like IDEs. More details about this
are [in the
FAQ](FAQ.md#why-is-meson-not-just-a-python-module-so-i-could-code-my-build-setup-in-python).
If because of this limitation you find yourself copying and pasting
code a lot you may be able to use a [`foreach` loop
instead](#foreach-statements).

## Stability Promises

Meson is very actively developed and continuously improved. There is a
possibility that future enhancements to the Meson build system will
require changes to the syntax. Such changes might be the addition of
new reserved keywords, changing the meaning of existing keywords or
additions around the basic building blocks like statements and
fundamental types. It is planned to stabilize the syntax with the 1.0
release.

## Grammar

This is the full Meson grammar, as it is used to parse Meson build definition files:

```
additive_expression: multiplicative_expression | (additive_expression additive_operator multiplicative_expression)
additive_operator: "+" | "-"
argument_list: positional_arguments ["," keyword_arguments] | keyword_arguments
array_literal: "[" [expression_list] "]"
assignment_statement: expression assignment_operator expression
assignment_operator: "=" | "+="
binary_literal: "0b" BINARY_NUMBER
BINARY_NUMBER: /[01]+/
boolean_literal: "true" | "false"
build_definition: (NEWLINE | statement)*
condition: expression
conditional_expression: logical_or_expression | (logical_or_expression "?" expression ":" assignment_expression
decimal_literal: DECIMAL_NUMBER
DECIMAL_NUMBER: /[1-9][0-9]*/
dictionary_literal: "{" [key_value_list] "}"
equality_expression: relational_expression | (equality_expression equality_operator relational_expression)
equality_operator: "==" | "!="
expression: conditional_expression | logical_or_expression
expression_list: expression ("," expression)*
expression_statement: expression
function_expression: id_expression "(" [argument_list] ")"
hex_literal: "0x" HEX_NUMBER
HEX_NUMBER: /[a-fA-F0-9]+/
id_expression: IDENTIFIER
IDENTIFIER: /[a-zA-Z_][a-zA-Z_0-9]*/
identifier_list: id_expression ("," id_expression)*
integer_literal: decimal_literal | octal_literal | hex_literal
iteration_statement: "foreach" identifier_list ":" id_expression NEWLINE (statement | jump_statement)* "endforeach"
jump_statement: ("break" | "continue") NEWLINE
key_value_item: expression ":" expression
key_value_list: key_value_item ("," key_value_item)*
keyword_item: id_expression ":" expression
keyword_arguments: keyword_item ("," keyword_item)*
literal: integer_literal | string_literal | boolean_literal | array_literal | dictionary_literal
logical_and_expression: equality_expression | (logical_and_expression "and" equality_expression)
logical_or_expression: logical_and_expression | (logical_or_expression "or" logical_and_expression)
method_expression: postfix_expression "." function_expression
multiplicative_expression: unary_expression | (multiplicative_expression multiplicative_operator unary_expression)
multiplicative_operator: "*" | "/" | "%"
octal_literal: "0o" OCTAL_NUMBER
OCTAL_NUMBER: /[0-7]+/
positional_arguments: expression ("," expression)*
postfix_expression: primary_expression | subscript_expression | function_expression | method_expression
primary_expression: literal | ("(" expression ")") | id_expression
relational_expression: additive_expression | (relational_expression relational_operator additive_expression)
relational_operator: ">" | "<" | ">=" | "<=" | "in" | ("not" "in")
selection_statement: "if" condition NEWLINE (statement)* ("elif" condition NEWLINE (statement)*)* ["else" (statement)*] "endif"
statement: (expression_statement | selection_statement | iteration_statement | assignment_statement) NEWLINE
string_literal: ("'" STRING_SIMPLE_VALUE "'") | ("'''" STRING_MULTILINE_VALUE "'''")
STRING_MULTILINE_VALUE: \.*?(''')\
STRING_SIMPLE_VALUE: \.*?(?<!\\)(\\\\)*?'\
subscript_expression: postfix_expression "[" expression "]"
unary_expression: postfix_expression | (unary_operator unary_expression)
unary_operator: "not" | "-"
```
