# Check that skip_rest only exits subdir, not the whole script.
# Should create an error because main.cpp does not exists.
project('example exit', 'cpp')

subdir('subdir')

message('Good')
executable('main', 'main.cpp')
