project('libtestprovider','c')

libtestprovider=static_library('testprovider',
    files('./provider.c'),
    install:true,
    c_args:['-Wall','-Werror'],
)

pkg = import('pkgconfig')


pkg.generate(
        name:'testprovider',
        filebase:'libtestprovider',
        description: 'fortest',
        requires: [],
        libraries_private: ['-Wl,--whole-archive'] +
            ['-L${libdir}','-l:libtestprovider.a']+
            ['-Wl,--no-whole-archive']
)
