extern "C" {
    fn g_hash_table_new() -> *mut std::ffi::c_void;
}

pub fn func() {
    unsafe {
        g_hash_table_new();
    }
}
