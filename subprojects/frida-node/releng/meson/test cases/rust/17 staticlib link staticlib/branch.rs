#[no_mangle]
pub extern "C" fn what_have_we_here() -> i32 {
    myleaf::HOW_MANY * myleaf::HOW_MANY
}
