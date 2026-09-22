project('myexe', 'c', version: '0.1')
sub = subproject('sub')

prog = find_program('foobar', version : '>= 2.0', required : false)
assert(not prog.found())

prog = find_program('foobar', version : '>= 1.0')
custom1 = custom_target('custom1',
                        build_by_default : true,
                        input : [],
                        output : 'main1.c',
                        command : [prog, '@OUTPUT@'])
gen = generator(prog,
                output : '@BASENAME@.c',
                arguments : ['@OUTPUT@'])
custom2 = gen.process('main2.input')

message(prog.full_path())

executable('e1', custom1)
executable('e2', custom2)
