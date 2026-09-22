use crate::bindings::GCond;
use crate::bindings::GMutex;
use crate::bindings::GPrivate;
use crate::bindings::GRWLock;
use crate::bindings::GRecMutex;
use crate::bindings::GSystemThread;
use crate::bindings::GThreadFunc;
use crate::bindings::{gpointer, gulong};
use alloc::boxed::Box;
use alloc::collections::BTreeMap;
use core::ffi::c_void;
use core::sync::atomic::{AtomicU32, AtomicU64, AtomicUsize, Ordering};

// Our implementation structures that overlay the opaque GLib structures

#[cfg_attr(target_pointer_width = "32", repr(C, align(4)))]
#[cfg_attr(target_pointer_width = "64", repr(C, align(8)))]
struct MutexImpl {
    lock: AtomicU32,
    _padding: [u8; core::mem::size_of::<GMutex>() - core::mem::size_of::<AtomicU32>()],
}

#[cfg_attr(target_pointer_width = "32", repr(C, align(4)))]
#[cfg_attr(target_pointer_width = "64", repr(C, align(8)))]
struct RecMutexImpl {
    owner: AtomicUsize,   // Thread ID of the owner (uses the 'p' field)
    count: AtomicU32,     // Recursion count (uses i[0])
    _unused: u32,         // Unused (uses i[1])
}

#[cfg_attr(target_pointer_width = "32", repr(C, align(4)))]
#[cfg_attr(target_pointer_width = "64", repr(C, align(8)))]
struct RWLockImpl {
    state: AtomicU32,     // Lock state (readers count + writer bit)
    _padding: [u8; core::mem::size_of::<GRWLock>() - core::mem::size_of::<AtomicU32>()],
}

// A generation counter rather than a flag: a flag stays set once signalled, so every
// later wait returns at once and the caller's loop spins instead of blocking.
#[cfg_attr(target_pointer_width = "32", repr(C, align(4)))]
#[cfg_attr(target_pointer_width = "64", repr(C, align(8)))]
struct CondImpl {
    generation: AtomicU32,
    _padding: [u8; core::mem::size_of::<GCond>() - core::mem::size_of::<AtomicU32>()],
}

const _: () = assert!(core::mem::size_of::<MutexImpl>() == core::mem::size_of::<GMutex>());
const _: () = assert!(core::mem::align_of::<MutexImpl>() == core::mem::align_of::<GMutex>());
const _: () = assert!(core::mem::size_of::<RecMutexImpl>() == core::mem::size_of::<GRecMutex>());
const _: () = assert!(core::mem::align_of::<RecMutexImpl>() == core::mem::align_of::<GRecMutex>());
const _: () = assert!(core::mem::size_of::<RWLockImpl>() == core::mem::size_of::<GRWLock>());
const _: () = assert!(core::mem::align_of::<RWLockImpl>() == core::mem::align_of::<GRWLock>());
const _: () = assert!(core::mem::size_of::<CondImpl>() == core::mem::size_of::<GCond>());
const _: () = assert!(core::mem::align_of::<CondImpl>() == core::mem::align_of::<GCond>());

// System thread implementation
#[repr(C)]
struct SystemThreadImpl {
    mutex: GMutex,            // Protects the thread state
    cond: GCond,              // Signals thread completion
    thread_id: AtomicU64,     // XNU thread ID
    func: GThreadFunc,        // Thread function
    data: gpointer,           // User data
    finished: AtomicU32,      // Whether thread has finished (0 = running, 1 = finished)
    detached: AtomicU32,      // Whether thread is detached (1) or joinable (0)
}

// Thread wrapper data passed to XNU kernel thread
#[repr(C)]
struct ThreadWrapperData {
    func: GThreadFunc,
    data: gpointer,
    system_thread: *mut SystemThreadImpl,
}

unsafe fn mutex_impl<'a>(mutex: *mut GMutex) -> &'a mut MutexImpl {
    unsafe { &mut *(mutex as *mut MutexImpl) }
}

unsafe fn rec_mutex_impl<'a>(rec_mutex: *mut GRecMutex) -> &'a mut RecMutexImpl {
    unsafe { &mut *(rec_mutex as *mut RecMutexImpl) }
}

unsafe fn rw_lock_impl<'a>(rw_lock: *mut GRWLock) -> &'a mut RWLockImpl {
    unsafe { &mut *(rw_lock as *mut RWLockImpl) }
}

unsafe fn cond_impl<'a>(cond: *mut GCond) -> &'a mut CondImpl {
    unsafe { &mut *(cond as *mut CondImpl) }
}

// TODO: Clean up Thread-Local Storage (TLS) entries when threads exit
static mut TLS_STORAGE: Option<Box<BTreeMap<(u64, *mut GPrivate), *mut c_void>>> = None;
static TLS_LOCK: AtomicU32 = AtomicU32::new(0);

#[unsafe(no_mangle)]
pub extern "C" fn g_mutex_init(mutex: *mut GMutex) {
    unsafe {
        let impl_ = mutex_impl(mutex);
        impl_.lock.store(0, Ordering::Relaxed);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_mutex_clear(_mutex: *mut GMutex) {}

#[unsafe(no_mangle)]
pub extern "C" fn g_mutex_lock(mutex: *mut GMutex) {
    unsafe {
        let impl_ = mutex_impl(mutex);
        lock_acquire(&impl_.lock);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_mutex_unlock(mutex: *mut GMutex) {
    unsafe {
        let impl_ = mutex_impl(mutex);
        lock_release(&impl_.lock);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_mutex_trylock(mutex: *mut GMutex) -> u32 {
    unsafe {
        let impl_ = mutex_impl(mutex);
        if lock_try_acquire(&impl_.lock) {
            1
        } else {
            0
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_rec_mutex_init(rec_mutex: *mut GRecMutex) {
    unsafe {
        let impl_ = rec_mutex_impl(rec_mutex);
        impl_.owner.store(0, Ordering::Relaxed);
        impl_.count.store(0, Ordering::Relaxed);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_rec_mutex_clear(_rec_mutex: *mut GRecMutex) {
}

#[unsafe(no_mangle)]
pub extern "C" fn g_rec_mutex_lock(rec_mutex: *mut GRecMutex) {
    unsafe {
        let impl_ = rec_mutex_impl(rec_mutex);

        loop {
            if rec_mutex_try_acquire(impl_) {
                break;
            }
            rec_mutex_wait(impl_);
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_rec_mutex_trylock(rec_mutex: *mut GRecMutex) -> u32 {
    unsafe {
        let impl_ = rec_mutex_impl(rec_mutex);

        if rec_mutex_try_acquire(impl_) {
            1
        } else {
            0
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_rec_mutex_unlock(rec_mutex: *mut GRecMutex) {
    unsafe {
        let impl_ = rec_mutex_impl(rec_mutex);

        loop {
            let current_count = impl_.count.load(Ordering::Relaxed);

            if current_count == 1 {
                if impl_.count
                    .compare_exchange_weak(1, 0, Ordering::Release, Ordering::Relaxed)
                    .is_ok()
                {
                    impl_.owner.store(0, Ordering::Relaxed);
                    wake_waiters(&impl_.count);
                    break;
                }
            } else {
                if impl_.count
                    .compare_exchange_weak(current_count, current_count - 1, Ordering::Release, Ordering::Relaxed)
                    .is_ok()
                {
                    break;
                }
            }
        }
    }
}

// Asking the kernel who we are costs more than the uncontended acquisition itself, and
// every allocation takes this path, so the identity is only fetched once the lock turns
// out to be held.
unsafe fn rec_mutex_try_acquire(impl_: &mut RecMutexImpl) -> bool {
    let current_count = impl_.count.load(Ordering::Relaxed);

    if current_count == 0 {
        if impl_.count
            .compare_exchange_weak(0, 1, Ordering::Acquire, Ordering::Relaxed)
            .is_ok()
        {
            impl_.owner.store(crate::kernel::current_thread_id() as usize, Ordering::Relaxed);
            return true;
        }
    } else {
        let current_owner = impl_.owner.load(Ordering::Relaxed);
        if current_owner == crate::kernel::current_thread_id() as usize {
            if impl_.count
                .compare_exchange_weak(
                    current_count,
                    current_count + 1,
                    Ordering::Acquire,
                    Ordering::Relaxed,
                )
                .is_ok()
            {
                return true;
            }
        }
    }
    false
}

unsafe fn rec_mutex_wait(impl_: &mut RecMutexImpl) {
    while impl_.count.load(Ordering::Relaxed) != 0 {
        wait_on(&impl_.count, &mut || impl_.count.load(Ordering::Relaxed) == 0);
    }
}

// Read-Write Lock implementation
// We use a simple scheme where the atomic value represents:
// - 0: unlocked
// - 1-0x7FFFFFFF: number of readers holding the lock
// - 0x80000000: writer lock held
const RW_WRITER_LOCK_BIT: u32 = 0x80000000;

#[unsafe(no_mangle)]
pub extern "C" fn g_rw_lock_init(rw_lock: *mut GRWLock) {
    unsafe {
        let impl_ = rw_lock_impl(rw_lock);
        impl_.state.store(0, Ordering::Relaxed);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_rw_lock_clear(_rw_lock: *mut GRWLock) {}

#[unsafe(no_mangle)]
pub extern "C" fn g_rw_lock_writer_lock(rw_lock: *mut GRWLock) {
    unsafe {
        let impl_ = rw_lock_impl(rw_lock);
        loop {
            if rw_lock_writer_try_acquire(impl_) {
                break;
            }
            rw_lock_wait_for_all(impl_);
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_rw_lock_writer_trylock(rw_lock: *mut GRWLock) -> u32 {
    unsafe {
        let impl_ = rw_lock_impl(rw_lock);
        if rw_lock_writer_try_acquire(impl_) {
            1
        } else {
            0
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_rw_lock_writer_unlock(rw_lock: *mut GRWLock) {
    unsafe {
        let impl_ = rw_lock_impl(rw_lock);
        impl_.state.store(0, Ordering::Release);
        wake_waiters(&impl_.state);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_rw_lock_reader_lock(rw_lock: *mut GRWLock) {
    unsafe {
        let impl_ = rw_lock_impl(rw_lock);
        loop {
            let current = impl_.state.load(Ordering::Relaxed);
            if current & RW_WRITER_LOCK_BIT != 0 {
                rw_lock_wait_for_writers(impl_);
                continue;
            }
            if impl_.state
                .compare_exchange_weak(current, current + 1, Ordering::Acquire, Ordering::Relaxed)
                .is_ok()
            {
                break;
            }
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_rw_lock_reader_trylock(rw_lock: *mut GRWLock) -> u32 {
    unsafe {
        let impl_ = rw_lock_impl(rw_lock);
        if rw_lock_reader_try_acquire(impl_) {
            1
        } else {
            0
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_rw_lock_reader_unlock(rw_lock: *mut GRWLock) {
    unsafe {
        let impl_ = rw_lock_impl(rw_lock);
        loop {
            let current = impl_.state.load(Ordering::Relaxed);
            if impl_.state
                .compare_exchange_weak(current, current - 1, Ordering::Release, Ordering::Relaxed)
                .is_ok()
            {
                if current - 1 == 0 {
                    wake_waiters(&impl_.state);
                }
                break;
            }
        }
    }
}

unsafe fn rw_lock_writer_try_acquire(impl_: &mut RWLockImpl) -> bool {
    impl_.state
        .compare_exchange_weak(0, RW_WRITER_LOCK_BIT, Ordering::Acquire, Ordering::Relaxed)
        .is_ok()
}

unsafe fn rw_lock_reader_try_acquire(impl_: &mut RWLockImpl) -> bool {
    let current = impl_.state.load(Ordering::Relaxed);
    if current & RW_WRITER_LOCK_BIT != 0 {
        return false;
    }
    impl_.state
        .compare_exchange_weak(current, current + 1, Ordering::Acquire, Ordering::Relaxed)
        .is_ok()
}

unsafe fn rw_lock_wait_for_writers(impl_: &mut RWLockImpl) {
    while impl_.state.load(Ordering::Relaxed) & RW_WRITER_LOCK_BIT != 0 {
        wait_on(&impl_.state, &mut || {
            impl_.state.load(Ordering::Relaxed) & RW_WRITER_LOCK_BIT == 0
        });
    }
}

unsafe fn rw_lock_wait_for_all(impl_: &mut RWLockImpl) {
    while impl_.state.load(Ordering::Relaxed) != 0 {
        wait_on(&impl_.state, &mut || impl_.state.load(Ordering::Relaxed) == 0);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_cond_init(cond: *mut GCond) {
    unsafe {
        let impl_ = cond_impl(cond);
        impl_.generation.store(0, Ordering::Relaxed);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_cond_clear(_cond: *mut GCond) {}

#[unsafe(no_mangle)]
pub extern "C" fn g_cond_wait(cond: *mut GCond, mutex: *mut GMutex) {
    unsafe {
        // Read the generation under the mutex, so a signal that lands after the
        // unlock below is still observed as a change.
        let seen = cond_impl(cond).generation.load(Ordering::Acquire);

        g_mutex_unlock(mutex);

        let impl_ = cond_impl(cond);
        while impl_.generation.load(Ordering::Acquire) == seen {
            wait_on(&impl_.generation, &mut || {
                impl_.generation.load(Ordering::Acquire) != seen
            });
        }

        g_mutex_lock(mutex);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_cond_signal(cond: *mut GCond) {
    unsafe {
        let impl_ = cond_impl(cond);
        impl_.generation.fetch_add(1, Ordering::Release);
        wake_waiters(&impl_.generation);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_cond_broadcast(cond: *mut GCond) {
    unsafe {
        let impl_ = cond_impl(cond);
        impl_.generation.fetch_add(1, Ordering::Release);
        wake_waiters(&impl_.generation);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_cond_wait_until(cond: *mut GCond, mutex: *mut GMutex, end_time: i64) -> u32 {
    unsafe {
        let seen = cond_impl(cond).generation.load(Ordering::Acquire);

        g_mutex_unlock(mutex);

        let impl_ = cond_impl(cond);
        let mut signalled = impl_.generation.load(Ordering::Acquire) != seen;
        while !signalled && crate::kernel::monotonic_micros() < end_time {
            wait_on(&impl_.generation, &mut || {
                impl_.generation.load(Ordering::Acquire) != seen
            });
            signalled = impl_.generation.load(Ordering::Acquire) != seen;
        }

        g_mutex_lock(mutex);

        if signalled { 1 } else { 0 }
    }
}

// A half that runs on a system with slots of its own says so, and every GPrivate takes one. The
// map below is what the kernel halves use, and it costs a lock and a lookup per call, which is
// what code the Stalker writes does on every block it leaves.
pub struct ThreadSlots {
    pub take: fn() -> u32,
    pub get: fn(u32) -> *mut c_void,
    pub set: fn(u32, *mut c_void),
}

pub fn use_thread_slots(slots: &'static ThreadSlots) {
    unsafe { SLOTS = Some(slots) };
}

fn slot_of(key: *mut GPrivate, slots: &ThreadSlots) -> u32 {
    let taken = unsafe { (*key).p } as usize;
    if taken != 0 {
        return (taken - 1) as u32;
    }

    unsafe {
        lock_acquire(&TLS_LOCK);

        let mut taken = (*key).p as usize;
        if taken == 0 {
            taken = ((slots.take)() + 1) as usize;
            (*key).p = taken as *mut c_void;
        }

        lock_release(&TLS_LOCK);

        (taken - 1) as u32
    }
}

fn slots() -> Option<&'static ThreadSlots> {
    unsafe { SLOTS }
}

static mut SLOTS: Option<&'static ThreadSlots> = None;

#[unsafe(no_mangle)]
pub extern "C" fn g_private_get(key: *mut GPrivate) -> *mut c_void {
    if let Some(slots) = slots() {
        return (slots.get)(slot_of(key, slots));
    }

    unsafe {
        let thread_id = crate::kernel::current_thread_id();
        lock_acquire(&TLS_LOCK);

        ensure_tls_storage();
        let storage_ptr = core::ptr::addr_of_mut!(TLS_STORAGE);
        let storage = (*storage_ptr).as_ref().unwrap();

        let value = storage
            .get(&(thread_id, key))
            .copied()
            .unwrap_or(core::ptr::null_mut());

        lock_release(&TLS_LOCK);
        value
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn g_private_set(key: *mut GPrivate, value: *mut c_void) {
    if let Some(slots) = slots() {
        (slots.set)(slot_of(key, slots), value);
        return;
    }

    unsafe {
        let thread_id = crate::kernel::current_thread_id();
        lock_acquire(&TLS_LOCK);

        ensure_tls_storage();
        let storage_ptr = core::ptr::addr_of_mut!(TLS_STORAGE);
        let storage = (*storage_ptr).as_mut().unwrap();

        storage.insert((thread_id, key), value);

        lock_release(&TLS_LOCK);
    }
}

unsafe fn ensure_tls_storage() {
    let storage_ptr = core::ptr::addr_of_mut!(TLS_STORAGE);
    unsafe {
        if (*storage_ptr).is_none() {
            *storage_ptr = Some(Box::new(BTreeMap::new()));
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn _g_system_thread_create(
    _stack_size: gulong,
    _name: *const core::ffi::c_char,
    func: GThreadFunc,
    data: gpointer,
) -> *mut GSystemThread {
    unsafe {
        crate::kernel::log("frida: glib is creating a thread\n\0");

        let system_thread = crate::kernel::alloc(core::mem::size_of::<SystemThreadImpl>()) as *mut SystemThreadImpl;

        g_mutex_init(&mut (*system_thread).mutex);
        g_cond_init(&mut (*system_thread).cond);
        (*system_thread).thread_id.store(0, Ordering::Relaxed);
        (*system_thread).func = func;
        (*system_thread).data = data;
        (*system_thread).finished.store(0, Ordering::Relaxed);
        (*system_thread).detached.store(0, Ordering::Relaxed);

        let wrapper_data = crate::kernel::alloc(core::mem::size_of::<ThreadWrapperData>()) as *mut ThreadWrapperData;
        (*wrapper_data).func = func;
        (*wrapper_data).data = data;
        (*wrapper_data).system_thread = system_thread;

        let _xnu_result = crate::kernel::spawn_thread(thread_wrapper, wrapper_data as *mut c_void);

        system_thread as *mut GSystemThread
    }
}

extern "C" fn thread_wrapper(parameter: *mut c_void, _wait_result: i32) {
    unsafe {
        let wrapper_data = parameter as *mut ThreadWrapperData;
        let func = (*wrapper_data).func;
        let data = (*wrapper_data).data;
        let system_thread = (*wrapper_data).system_thread;

        let current_thread_id = crate::kernel::current_thread_id();
        (*system_thread).thread_id.store(current_thread_id, Ordering::Release);
        wake_waiters(&(*system_thread).thread_id);

        func.unwrap()(data);

        g_mutex_lock(&mut (*system_thread).mutex);
        (*system_thread).finished.store(1, Ordering::Relaxed);
        g_cond_signal(&mut (*system_thread).cond);

        let detached = (*system_thread).detached.load(Ordering::Relaxed);
        g_mutex_unlock(&mut (*system_thread).mutex);

        if detached != 0 {
            cleanup_system_thread(system_thread);
        }

        cleanup_wrapper_data(wrapper_data);
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn _g_system_thread_detach(thread: *mut GSystemThread) {
    unsafe {
        let system_thread = thread as *mut SystemThreadImpl;

        g_mutex_lock(&mut (*system_thread).mutex);
        (*system_thread).detached.store(1, Ordering::Relaxed);

        let finished = (*system_thread).finished.load(Ordering::Relaxed);
        g_mutex_unlock(&mut (*system_thread).mutex);
        if finished != 0 {
            cleanup_system_thread(system_thread);
        }
    }
}

#[unsafe(no_mangle)]
pub extern "C" fn _g_system_thread_wait(thread: *mut GSystemThread) {
    unsafe {
        let system_thread = thread as *mut SystemThreadImpl;

        while (*system_thread).thread_id.load(Ordering::Acquire) == 0 {
            wait_on(&(*system_thread).thread_id, &mut || {
                (*system_thread).thread_id.load(Ordering::Acquire) != 0
            });
        }

        g_mutex_lock(&mut (*system_thread).mutex);
        while (*system_thread).finished.load(Ordering::Relaxed) == 0 {
            g_cond_wait(&mut (*system_thread).cond, &mut (*system_thread).mutex);
        }
        g_mutex_unlock(&mut (*system_thread).mutex);

        cleanup_system_thread(system_thread);
    }
}

unsafe fn cleanup_system_thread(system_thread: *mut SystemThreadImpl) {
    crate::kernel::free(system_thread as *mut u8, core::mem::size_of::<SystemThreadImpl>());
}

unsafe fn cleanup_wrapper_data(wrapper_data: *mut ThreadWrapperData) {
    crate::kernel::free(wrapper_data as *mut u8, core::mem::size_of::<ThreadWrapperData>());
}

unsafe fn lock_acquire(lock: &AtomicU32) {
    while !unsafe { lock_try_acquire(lock) } {
        wait_on(lock, &mut || lock.load(Ordering::Relaxed) == 0);
    }
}

unsafe fn lock_try_acquire(lock: &AtomicU32) -> bool {
    lock.compare_exchange_weak(0, 1, Ordering::Acquire, Ordering::Relaxed)
        .is_ok()
}

unsafe fn lock_release(lock: &AtomicU32) {
    lock.store(0, Ordering::Release);
    wake_waiters(lock);
}

// Sleeps until `ready` holds or the slice expires, whichever comes first.
//
// These used to spin on wfe/sev. A kernel thread that spins never reaches the
// scheduler, so a lost wakeup — or a genuine deadlock, which is how we found this —
// starves the CPU it is on until the hardware watchdog takes the machine down. The
// bounded sleep turns both into a stall that can be observed instead.
//
// The address of the word being waited on names the wait channel: XNU pairs
// assert_wait()/thread_wakeup() on it directly, while the Linux shim has a single
// channel and treats it as a hint. Every caller re-checks its own condition in a
// loop, so a spurious wakeup costs nothing either way.
fn wait_on<T>(word: &T, ready: &mut dyn FnMut() -> bool) {
    crate::kernel::wait(token_of(word), Some(WAIT_SLICE_US), ready);

    // The wait above returns without sleeping whenever `ready` already holds, so on
    // its own it is not enough to keep a contended loop off the CPU: every caller
    // here loops, and a loop in a kernel thread that never reaches the scheduler
    // takes the machine down rather than merely running hot. Yield unconditionally.
    crate::kernel::yield_now();
}

fn wake_waiters<T>(word: &T) {
    crate::kernel::wake(token_of(word));
}

fn token_of<T>(word: &T) -> *const u8 {
    word as *const T as *const u8
}

const WAIT_SLICE_US: u64 = 1000;
