# Test https://github.com/mesonbuild/meson/issues/2096
# Note that removing 'shnodep' from link_with: makes the error go away because
# then it is added after the static library is added to the link command.
test('shared-static', executable('shstexe', 'shstmain.c', link_with : [shnodep, stshdep]))

# Static library that needs a symbol defined in an object file. This already
# works, but good to add a test case early.
stodep = static_library('stodep', 'libsto.c')
test('stodep', executable('stodep', 'stomain.c', 'stobuilt.c', link_with : stodep))
