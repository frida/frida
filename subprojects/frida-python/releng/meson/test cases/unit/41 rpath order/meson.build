project('myexe', 'c')

sub1 = subproject('sub1')
sub1_dep = sub1.get_variable('sub1_dep')

sub2 = subproject('sub2')
sub2_dep = sub2.get_variable('sub2_dep')

executable('myexe',
           'myexe.c',
           dependencies: [sub1_dep, sub2_dep])
