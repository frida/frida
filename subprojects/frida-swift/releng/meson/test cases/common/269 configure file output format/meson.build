project('configure file output format')

data = configuration_data()
data.set_quoted('str', 'hello world!', description: '''This is
a multiline
description''')
data.set('unquoted', 'float')
data.set('int', 42, description: 'ultimate question of life, the universe, and everything')
data.set('bool', true)
data.set('false', false)

config_h = configure_file(
    configuration: data,
    output_format: 'c',
    output: 'config.h'
)

config_nasm = configure_file(
    configuration: data,
    output_format: 'nasm',
    output: 'config.nasm'
)

config_json = configure_file(
    configuration: data,
    output_format: 'json',
    output: 'config.json'
)

config_mg = configure_file(
    configuration: data,
    macro_name: 'CONFIG_MAGNESIUM_H',
    output_format: 'c',
    output: 'config_mg.h'
)

py = find_program('python3')
compare_py = files('compare.py')
expected_config_h = files('expected/config.h')
expected_config_nasm = files('expected/config.nasm')
expected_config_json = files('expected/config.json')
expected_config_mg = files('expected/config.mg')

test('c_output', py, args: [compare_py, expected_config_h, config_h])
test('nasm_output', py, args: [compare_py, expected_config_nasm, config_nasm])
test('json_output', py, args: [compare_py, expected_config_json, config_json])
test('c_mg_output', py, args: [compare_py, expected_config_mg, config_mg])
