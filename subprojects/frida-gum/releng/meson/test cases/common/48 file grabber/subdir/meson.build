sc = run_command(grabber, check: true)
subsources = sc.stdout().strip().split(newline)

se = executable('subprog', subsources)
test('subgrabtest', se)
