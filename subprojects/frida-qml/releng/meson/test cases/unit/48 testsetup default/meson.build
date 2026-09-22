project('testsetup default', 'c')

envcheck = find_program('envcheck.py')

# Defining ENV_A in test-env should overwrite ENV_A from test setup
env_1 = environment()
env_1.set('ENV_A', '1')
test('test-env', envcheck, env: env_1)

# Defining default env which is used unless --setup is given or the
# env variable is defined in the test.
env_2 = environment()
env_2.set('ENV_A', '2')
env_2.set('ENV_B', '2')
env_2.set('ENV_C', '2')
add_test_setup('mydefault', env: env_2, is_default: true)

# Defining a test setup that will update some of the env variables
# from the default test setup.
env_3 = env_2
env_3.set('ENV_A', '3')
env_3.set('ENV_B', '3')
add_test_setup('other', env: env_3)
