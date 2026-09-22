project('release', 'cpp', 'cuda', version : '1.0.0', default_options : ['buildtype=release', 'b_ndebug=if-release'])

# We don't actually need boost, but it serves as a common dependency
# that has the potential to add "-isystem/usr/include" to the compile
# line. By making it optional, we test that system search paths get
# removed without unnecessarily failing the test if boost is absent.
boost_dep = dependency('boost', include_type : 'system', required : false)

exe = executable('prog', 'main.cu', dependencies : boost_dep)
test('cudatest', exe)
