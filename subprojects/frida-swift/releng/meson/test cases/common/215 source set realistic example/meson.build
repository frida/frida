# a sort-of realistic example that combines the sourceset and keyval
# modules, inspired by QEMU's build system

project('sourceset-example', 'cpp', default_options: ['cpp_std=c++11'])

cppid = meson.get_compiler('cpp').get_id()
if cppid == 'pgi'
  error('MESON_SKIP_TEST: Even PGI 19.4 that claims C++17 full support, cannot handle auto x = y syntax used in this test.')
endif

ss = import('sourceset')
keyval = import('keyval')

zlib = declare_dependency(compile_args: '-DZLIB=1')
another = declare_dependency(compile_args: '-DANOTHER=1')
not_found = dependency('not-found', required: false, method : 'pkg-config')

common = ss.source_set()
specific = ss.source_set()

common.add(files('main.cc'))
common.add(when: [zlib, another], if_true: files('zlib.cc'))
common.add(when: not_found,
           if_true: files('was-found.cc'),
           if_false: files('not-found.cc'))

subdir('boards')
subdir('devices')

if meson.is_unity()
  specific.add_all(common)
  common = ss.source_set()
  common.add(files('dummy.cpp'))
endif

common_lib = static_library('common', common.all_sources(),
                            dependencies: common.all_dependencies())

targets = [ 'arm', 'aarch64', 'x86' ]
target_dirs = { 'arm' : 'arm', 'aarch64' : 'arm', 'x86': 'x86' }

foreach x : targets
  config = keyval.load('config' / x)
  target_specific = specific.apply(config, strict: false)
  target_common = common.apply(config, strict: false)
  target_deps = target_specific.dependencies() + target_common.dependencies()
  executable(x,
             objects: common_lib.extract_objects(target_common.sources()),
             sources: target_specific.sources(),
             dependencies: target_deps,
             include_directories: 'boards' / target_dirs[x],
             cpp_args: '-DTHE_TARGET="' + x + '"')
endforeach
