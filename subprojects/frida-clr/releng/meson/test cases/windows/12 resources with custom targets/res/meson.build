win = import('windows')

rc_writer = find_program('./gen-res.py')

rc_sources = []

foreach id : [1, 2]
    rc_sources += custom_target('RC source file @0@'.format(id),
        input : 'myres.rc.in',
        output : 'myres_@0@.rc'.format(id),
        command : [rc_writer, '@INPUT@', '@OUTPUT@', files('sample.ico')],
        install : false,
        build_always : false)
endforeach

rc_sources += files('myres_static.rc')

res = win.compile_resources(rc_sources,
    include_directories: include_directories('.'))
