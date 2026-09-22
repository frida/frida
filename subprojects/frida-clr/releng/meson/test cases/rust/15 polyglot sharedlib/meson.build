project('adder', 'c', 'rust', version: '1.0.0')

subdir('zero')

rl = shared_library('radder', 'adder.rs',
  rust_abi: 'c',
  link_with: [zero_shared, zero_static])

l = shared_library('adder', 'adder.c',
  c_args: '-DBUILDING_ADDER',
  link_with: rl,
  version: '1.0.0',
  soversion: '1',
)
test('adder', executable('addertest', 'addertest.c', link_with: l))
