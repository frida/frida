project('D has arguments test', 'd')

compiler = meson.get_compiler('d')

assert(compiler.compiles('int i;'), 'Basic code test does not compile: ' + compiler.get_id())
assert(compiler.has_multi_arguments(['-I.', '-J.']), 'Multi argument test does not work: ' + compiler.get_id())
assert(compiler.has_argument('-I.'), 'Basic argument test does not work: ' + compiler.get_id())
assert(compiler.has_argument('-flag_a_d_compiler_definitely_does_not_have') == false, 'Basic argument test does not work: ' + compiler.get_id())
