project('rust static library', 'rust', 'c')

o = static_library('other', 'other.rs')
v = static_library('value', 'value.c')
l = static_library('stuff', 'stuff.rs', link_whole : [o, v], install : true)
e = executable('prog', 'prog.rs', link_with : l, install : true)
test('linktest', e)
