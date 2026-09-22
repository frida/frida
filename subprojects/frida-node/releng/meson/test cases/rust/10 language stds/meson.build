project('rust std options', 'rust')

# this only works in 2018
new = executable(
  'new',
  '2018.rs',
  override_options : ['rust_std=2018'],
)

# this only works in 2015
old = static_library(
  'old',
  '2015.rs',
  override_options : ['rust_std=2015'],
)


test('2018 std', new)
