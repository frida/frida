ico_writer = find_program('gen-ico.py')

ico = custom_target('makeico',
        input : 'sample.ico.in',
        output : 'sample.ico',
        command : [ico_writer, '@INPUT@', '@OUTPUT@'],
        install : false,
        build_always : false)
