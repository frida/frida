project('configuration_data is immutable')

a = configuration_data()

configure_file(
    configuration : a,
    input : 'input',
    output : 'output',
)

still_immutable = a
still_immutable.set('hello', 'world')
