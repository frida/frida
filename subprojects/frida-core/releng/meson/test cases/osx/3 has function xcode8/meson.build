project('has function xcode8', 'c')

cc = meson.get_compiler('c')

# XCode 8 location for the macOS 10.12 SDK
sdk_args = ['-isysroot', '/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk']
args_10_11 = ['-mmacosx-version-min=10.11'] + sdk_args
args_10_12 = ['-mmacosx-version-min=10.12'] + sdk_args

# XCode 9 location for the macOS 10.13 SDK
sdk_args = ['-isysroot', '/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.13.sdk']
args_10_12 = ['-mmacosx-version-min=10.13'] + sdk_args

# Test requires XCode 8 which has the MacOSX 10.12 SDK
if cc.version().version_compare('>=8.0') and cc.version().version_compare('<8.1')
  if cc.has_function('clock_gettime', args : args_10_11, prefix : '#include <time.h>')
    error('Should not have found clock_gettime via <time.h> when targeting Mac OS X 10.11')
  endif
  if not cc.has_function('clock_gettime', args : args_10_12, prefix : '#include <time.h>')
    error('Did NOT find clock_gettime via <time.h> when targeting Mac OS X 10.12')
  endif
  if not cc.has_function('clock_gettime', args : args_10_11)
    error('Did NOT find clock_gettime w/o a prototype when targeting Mac OS X 10.11')
  endif
  if not cc.has_function('clock_gettime', args : args_10_12)
    error('Did NOT find clock_gettime w/o a prototype when targeting Mac OS X 10.12')
  endif
else
  error('MESON_SKIP_TEST Test needs XCode 8.')
endif
