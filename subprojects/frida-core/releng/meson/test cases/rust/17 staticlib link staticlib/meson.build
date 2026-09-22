project('staticlib link staticlib', 'c', 'rust')

leaf = static_library('leaf', 'leaf.rs', rust_crate_type : 'rlib')
# Even though leaf is linked using link_with, this gets implicitly promoted to link_whole because
# it is an internal Rust project.
branch = static_library('branch', 'branch.rs', link_with: leaf, rust_crate_type : 'staticlib', rust_dependency_map : { 'leaf' : 'myleaf' }, install : true)
e = executable('prog', 'prog.c', link_with : branch, install : true)
test('linktest', e)
