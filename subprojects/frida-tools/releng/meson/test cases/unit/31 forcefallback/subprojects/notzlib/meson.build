project('notzlib', 'c')

notzlib_sources = ['notzlib.c']

notzlib = library('notzlib', notzlib_sources)

zlib_dep = declare_dependency(link_with: notzlib, include_directories: include_directories(['.']))
