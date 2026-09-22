project('rust private crate collision', 'rust')

l = static_library('rand', 'rand.rs', install : true)
e = executable('prog', 'prog.rs', link_with : l, install : true)
test('linktest', e)
