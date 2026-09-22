if get_option('graft_tool') \
    .disable_auto_if(meson.is_subproject()) \
    .allowed()
  executable('gum-graft', 'gumgraft.c',
    dependencies: [gum_dep],
    install: true,
  )
endif
