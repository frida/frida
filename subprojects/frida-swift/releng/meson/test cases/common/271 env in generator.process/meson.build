project('test_env_in_generator_process', 'c')

generate_main_py = find_program('generate_main.py')

main_generator = generator(generate_main_py,
    arguments: ['@INPUT@', '@OUTPUT@'],
    output: '@BASENAME@' + '.c'
)

main_template = files('main.template')

# With explicit values
my_executable = executable('myexecutable', main_generator.process(main_template, env: {'ENV_VAR_VALUE': '0'}))
test('explicit_value', my_executable)

# With env object
env = environment()
env.set('ENV_VAR_VALUE', '0')

my_executable2 = executable('myexecutable2', main_generator.process(main_template, env: env))
test('env_object', my_executable2)
