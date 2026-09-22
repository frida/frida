# Should run, even though main.cpp does not exist and we call error in the last line.
# subdir_done jumps to end, so both lines are not executed.

project('example exit')

if true
  subdir_done()
endif

executable('main', 'main.cpp')
error('Unreachable')
