project('C# library', 'cs')

python3 = import('python3').find_python()
generated_sources = custom_target('gen_sources',
    input: 'helper.cs',
    output: 'helper.cs',
    command: [python3, '-c',
      'import shutil, sys; shutil.copyfile(sys.argv[1], sys.argv[2])',
      '@INPUT@', '@OUTPUT@']
)

l = shared_library('helper', generated_sources, install : true)

e = executable('prog', 'prog.cs', link_with : l, install : true)
test('libtest', e)
