project('cmakeSubTest', ['c', 'cpp'])

cm = import('cmake')

sub_pro = cm.subproject('nothing', required: false)
assert(not sub_pro.found(), 'subproject found() reports wrong value')
