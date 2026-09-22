project('install a whole subdir',
  default_options : ['install_umask=preserve'])

# A subdir with an exclusion:
install_subdir('sub2',
  exclude_files : ['excluded-three.dat'],
  exclude_directories : ['excluded'],
  install_dir : 'share')

# More exclusions
install_subdir('sub3',
  exclude_files : ['data/excluded.txt'],
  exclude_directories : ['data/excluded'],
  install_dir : 'share')

subdir('subdir')
# A subdir with write perms only for the owner
# and read-list perms for owner and group
install_subdir('sub1', install_dir : 'share', install_mode : ['rwxr-x--t', 'root'])
install_subdir('sub/sub1', install_dir : 'share')

# strip_directory
install_subdir('sub_elided', install_dir : 'share', strip_directory : true)
install_subdir('nested_elided/sub', install_dir : 'share', strip_directory : true)

# Create new empty directory that doesn't exist in the source tree
install_subdir('new_directory', install_dir : 'share')
