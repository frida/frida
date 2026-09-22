project('library with name_prefix name_suffix test', 'c')

shared_library('foo', 'libfile.c', name_prefix: '', install : true)
static_library('bar', 'libfile.c', name_prefix: '', install : true)

shared_library('baz', 'libfile.c', name_suffix: 'cheese', install : true)
static_library('qux', 'libfile.c', name_suffix: 'cheese', install : true)

shared_library('corge', 'libfile.c', name_prefix: 'bow', name_suffix: 'stern', install : true)
static_library('grault', 'libfile.c', name_prefix: 'bow', name_suffix: 'stern', install : true)

# exercise default name_prefix and name_suffix
shared_library('garply', 'libfile.c', name_prefix: [], name_suffix: [], install : true)
