dep3_libs = []

# Permutate all combinations of shared and static libraries up to three levels
# executable -> shared -> static -> shared (etc)
foreach dep2 : ['sh', 'st']
  foreach dep1 : ['sh', 'st']
    foreach libtype : ['sh', 'st']
      name = libtype + dep1 + dep2
      if dep2 == 'sh'
        libret = 1
      elif dep2 == 'st'
        libret = 2
      else
        error('Unknown dep2 "@0@"'.format(dep2))
      endif

      if libtype == 'sh'
        target = 'shared_library'
        build_args = []
      elif libtype == 'st'
        target = 'static_library'
        build_args = ['-DMESON_STATIC_BUILD']
      else
        error('Unknown libtype "@0@"'.format(libtype))
      endif

      cdata = configuration_data()
      cdata.set('DEPENDENCY', dep1 + dep2)
      cdata.set('LIBTYPE', libtype)
      cdata.set('VALUE', libret)

      lib_c = configure_file(input : 'lib.c.in',
                             output : name + '-lib.c',
                             configuration : cdata)
      dep = get_variable(dep1 + dep2 + 'dep')
      dep3_lib = build_target(name, lib_c, link_with : dep,
                              target_type : target,
                              c_args : build_args)
      dep3_libs += [dep3_lib]

      main_c = configure_file(input : 'main.c.in',
                              output : name + '-main.c',
                              configuration : cdata)
      dep3_bin = executable(name + '_test', main_c, link_with : dep3_lib,
                            c_args : build_args)
      test(name + 'test', dep3_bin)
    endforeach
  endforeach
endforeach
