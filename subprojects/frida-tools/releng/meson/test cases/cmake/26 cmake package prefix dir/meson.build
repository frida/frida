project('cmakePackagePrefixDir', 'c', version: '1.0.0')

cmake = import('cmake')

cmake.configure_package_config_file(
    name: 'cmakePackagePrefixDir',
    input: 'cmakePackagePrefixDirConfig.cmake.in',
    configuration: configuration_data(),
)

# NOTE: can't use fs.read because cmakePackagePrefixDirConfig.cmake is in build_dir
python = find_program('python3')
lines = run_command(python, '-c',
    '[print(line, end="") for line in open("@0@")]'.format(meson.current_build_dir() / 'cmakePackagePrefixDirConfig.cmake'), check : true,
).stdout().split('\n')

message(lines)

assert(lines[5] == 'get_filename_component(PACKAGE_PREFIX_DIR "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)')
