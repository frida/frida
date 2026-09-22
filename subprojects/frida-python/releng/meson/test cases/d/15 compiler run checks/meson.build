project('test-d-run-checks', 'd')

dc = meson.get_compiler('d')

run_sizeof = dc.run('int main() { return (void*).sizeof; }')
if run_sizeof.returncode() == 8
    run_versions = ['Is64bits']
elif run_sizeof.returncode() == 4
    run_versions = ['Is32bits']
endif
run_sizeof_exe = executable('run_sizeof', 'test_sizeof.d',
    d_module_versions: run_versions,
)
test('test D compiler run', run_sizeof_exe)

sizeof_sizeof = dc.sizeof('void*')
if sizeof_sizeof == 8
    run_versions = ['Is64bits']
elif sizeof_sizeof == 4
    run_versions = ['Is32bits']
endif
sizeof_sizeof_exe = executable('sizeof_sizeof', 'test_sizeof.d',
    d_module_versions: run_versions,
)
test('test D compiler sizeof', sizeof_sizeof_exe)

if not dc.has_header('std.stdio')
    error('Could not find std.stdio import')
endif

if dc.has_header('not_a_d_module')
    error('has_header inconsistent result')
endif

# same checks as C/C++ alignments (D has same alignment requirements as C)

# These tests should return the same value on all
# platforms. If (and when) they don't, fix 'em up.
if dc.alignment('char') != 1
error('Alignment of char misdetected.')
endif

dbl_alignment = dc.alignment('double')

if dbl_alignment == 8 or dbl_alignment == 4
message('Alignment of double ok.')
else
error('Alignment of double misdetected.')
endif

