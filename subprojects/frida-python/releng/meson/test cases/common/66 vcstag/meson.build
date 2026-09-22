project('vcstag', 'c')

version_src = vcs_tag(input : 'vcstag.c.in',
output : 'vcstag.c',
fallback : '1.0.0')

version_src_custom = vcs_tag(input : 'vcstag.c.in',
output : 'vcstag-custom.c',
command : ['git', 'show-ref', '-s', 'refs/heads/master'],
fallback : '1.0.0')

version_src_notfound_fallback = vcs_tag(input : 'vcstag.c.in',
output : 'vcstag-notfound-fallback.c',
command : ['git-but-not-found-sorry', 'show-ref', '-s', 'refs/heads/master'],
fallback : '1.0.0')

version_src_fallback = vcs_tag(input : 'vcstag.c.in',
output : 'vcstag-fallback.c')

git = find_program('git')

version_src_git_program = vcs_tag(input : 'vcstag.c.in',
output : 'vcstag-git-program.c',
command : [git, 'rev-parse', 'HEAD'],
fallback : '1.0.0')

version_src_file = vcs_tag(input : 'vcstag.c.in',
output : 'vcstag-file.c',
command : files('version.py'))

tagprog = executable('tagprog', 'tagprog.c', version_src)

version_src_executable = vcs_tag(input : 'vcstag.c.in',
output : 'vcstag-executable.c',
command : [tagprog])

executable('tagprog-custom', 'tagprog.c', version_src_custom)
executable('tagprog-fallback', 'tagprog.c', version_src_fallback)
executable('tagprog-notfound-fallback', 'tagprog.c', version_src_notfound_fallback)
executable('tagprog-git-program', 'tagprog.c', version_src_git_program)
executable('tagprog-executable', 'tagprog.c', version_src_executable)
executable('tagprog-file', 'tagprog.c', version_src_file)
