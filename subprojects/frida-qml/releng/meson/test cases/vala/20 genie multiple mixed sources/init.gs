def extern c_test_one_is_true():bool
def extern c_test_two_is_true():bool

init
	assert( new Genie.TestOne().is_true() )
	assert( new Genie.TestTwo().is_true() )
	assert( new Vala.TestOne().is_true() )
	assert( new Vala.TestTwo().is_true() )
	assert( c_test_one_is_true() )
	assert( c_test_two_is_true() )
