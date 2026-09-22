project(
    'environment',
    meson_version: '>=1.4.0',
)

testenv = find_program(files('testenv.py'))


env = environment()
env.unset('foo')
test('not set', testenv, args: ['foo'], env: env)

testcase expect_error('You cannot set the already unset variable \'foo\'')
    env.set('foo', 'bar')
endtestcase

testcase expect_error('You cannot append to unset variable \'foo\'')
    env.append('foo', 'bar')
endtestcase

testcase expect_error('You cannot prepend to unset variable \'foo\'')
    env.prepend('foo', 'bar')
endtestcase


env1 = environment('foo=bar', method: 'append', separator: ':')
env1.append('foo', 'baz', separator: ':')
test('append', testenv, args: ['foo', 'bar:baz'], env: env1)

testcase expect_error('You cannot unset the \'foo\' variable because it is already set')
    env1.unset('foo')
endtestcase


env2 = environment(['foo=baz'], method: 'prepend', separator: ':')
env2.prepend('foo', 'bar', separator: ':')
test('prepend', testenv, args: ['foo', 'bar:baz'], env: env2)

testcase expect_error('You cannot unset the \'foo\' variable because it is already set')
    env2.unset('foo')
endtestcase


env3 = environment({'foo': 'foobar'}, method: 'set', separator: ':')
env3.set('foo', 'qux')
test('reset', testenv, args: ['foo', 'qux'], env: env3)

testcase expect_error('You cannot unset the \'foo\' variable because it is already set')
    env3.unset('foo')
endtestcase
