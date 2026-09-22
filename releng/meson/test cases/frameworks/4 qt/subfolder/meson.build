
simple_gen = find_program('generator.py', required : true)

txt_resource = custom_target('txt_resource',
  output : 'txt_resource.txt',
  command : [simple_gen, '@OUTPUT@'],
)

cfg = configuration_data()

cfg.set('filepath', meson.current_source_dir()+'/../thing2.png')
cfg.set('txt_resource', txt_resource.full_path())
# here we abuse the system by guessing build dir layout
cfg.set('txt_resource2', 'txt_resource.txt')


rc_file = configure_file(
  configuration : cfg,
  input : 'resources/stuff4.qrc.in',
  output : 'stuff4.qrc',
)

extra_cpp_args = []
if meson.is_unity()
  extra_cpp_args += '-DUNITY_BUILD'
  qresources = qtmodule.preprocess(qt + '_subfolder_unity_resource',qresources : ['resources/stuff3.qrc', rc_file])
else
  qresources = qtmodule.preprocess(qresources : ['resources/stuff3.qrc', rc_file])
endif

app = executable('subfolder', 'main.cpp', qresources, dependencies : qtdep, cpp_args: extra_cpp_args)
test(qt + 'subfolder', app)
