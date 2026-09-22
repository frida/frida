project('custom target install data')

preproc = find_program('preproc.py')

t = custom_target('Info.plist',
    command: [preproc, '@INPUT@', '@OUTPUT@'],
    input: 'Info.plist.cpp',
    output: 'Info.plist',
)

install_data(t)
