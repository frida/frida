project('pch test', 'c', 'cpp',
  meson_version: '>= 0.46.0')

cc = meson.get_compiler('c')
cc_id = cc.get_id()

if cc_id == 'pgi'
  error('MESON_SKIP_TEST: PGI compiler does support PCH, however, PGI cannot tolerate spaces in the --pch_dir path and Meson run_project_tests.py uses spaces in temporary build path names. If this test is run individually with no spaces in build path, it will pass.')
endif

subdir('c')
subdir('cpp')
subdir('generated')
subdir('userDefined')
subdir('withIncludeDirectories')
if meson.backend() == 'xcode'
  warning('Xcode backend does not support forced includes. Skipping "withIncludeFile" which requires this.')
else
  subdir('withIncludeFile')
endif

if meson.backend() == 'xcode'
  warning('Xcode backend only supports one precompiled header per target. Skipping "mixed" which has various precompiled headers.')
else
  subdir('mixed')
endif

if cc_id == 'msvc'
  subdir('linkwhole')
endif
