project('non-ascii to ascii encoding')
# Writing a non-ASCII character with a ASCII encoding should fail
conf9 = configuration_data()
conf9.set('var', 'д')
configure_file(
  input : 'config9.h.in',
  output : '@BASENAME@',
  encoding : 'ascii',
  configuration : conf9
)
