use core::ffi::{c_int, c_long, c_void};
use core::ptr;

use crate::bindings::{
    gum_barebone_handle_exception, GumCpuContext,
    _GumExceptionType_GUM_EXCEPTION_ACCESS_VIOLATION as ACCESS_VIOLATION,
};

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[repr(C)]
struct NotifierBlock {
    notifier_call: Option<unsafe extern "C" fn(*mut NotifierBlock, c_long, *mut c_void) -> c_int>,
    next: *mut NotifierBlock,
    priority: c_int,
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
unsafe impl Sync for NotifierBlock {}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
static mut REPORTER: NotifierBlock =
    NotifierBlock { notifier_call: None, next: ptr::null_mut(), priority: 0 };

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
#[repr(C)]
struct DieArgs {
    regs: *mut c_void,
    message: *const u8,
    err: c_long,
    trapnr: c_int,
    signr: c_int,
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
const DIE_OOPS: c_long = 1;
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
const NOTIFY_DONE: c_int = 0;
#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
const NOTIFY_STOP: c_int = 0x8001;

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
unsafe fn dispatch(action: c_long, data: *mut c_void) -> c_int {
    if action != DIE_OOPS {
        return NOTIFY_DONE;
    }
    let regs = unsafe { (*(data as *const DieArgs)).regs };
    if unsafe { recovered(regs) } {
        return NOTIFY_STOP;
    }
    NOTIFY_DONE
}

#[cfg(target_arch = "x86")]
static mut RESUME_CONTEXT: GumCpuContext = unsafe { core::mem::zeroed() };

#[cfg(target_arch = "x86")]
static mut RESUME_STUB: usize = 0;

#[cfg(target_arch = "x86")]
const RESUME_SCRATCH_SIZE: usize = 512;

#[cfg(target_arch = "x86")]
static mut RESUME_SCRATCH: [u8; RESUME_SCRATCH_SIZE] = [0; RESUME_SCRATCH_SIZE];

#[cfg(target_arch = "x86")]
pub fn install() {
    unsafe { build_resume_stub() };
    unsafe {
        (&raw mut REPORTER).write(NotifierBlock {
            notifier_call: Some(frida_kcb_die),
            next: ptr::null_mut(),
            priority: 0,
        });
        frida_k_register_die_notifier(&raw mut REPORTER as *mut c_void);
    }
}

#[cfg(target_arch = "x86")]
pub fn uninstall() {
    unsafe { frida_k_unregister_die_notifier(&raw mut REPORTER as *mut c_void) };
}

#[cfg(target_arch = "x86")]
unsafe fn build_resume_stub() {
    let stub = crate::kernel::alloc_code(16);
    if stub.is_null() {
        return;
    }
    unsafe {
        stub.write(0xbf);
        (stub.add(1) as *mut u32).write((&raw mut RESUME_CONTEXT) as u32);
        stub.add(5).write(0xe9);
        let rel = (frida_i386_resume as usize).wrapping_sub(stub as usize + 10) as u32;
        (stub.add(6) as *mut u32).write(rel);
        RESUME_STUB = stub as usize;
    }
}

#[cfg(target_arch = "x86")]
unsafe fn recovered(regs: *mut c_void) -> bool {
    let faulted_at = unsafe { (regs as *const u32).add(12).read() as usize };
    let (base, size) = crate::own_range();
    if faulted_at < base || faulted_at >= base + size {
        return false;
    }

    let mut context = unsafe { context_of(regs) };
    let handled = unsafe {
        gum_barebone_handle_exception(
            ACCESS_VIOLATION,
            faulted_at as *mut c_void,
            faulting_address() as *mut c_void,
            &mut context,
        )
    };
    if handled == 0 || unsafe { RESUME_STUB } == 0 {
        return false;
    }

    unsafe {
        (&raw mut RESUME_CONTEXT).write(context);
        (regs as *mut u32).add(12).write(RESUME_STUB as u32);
    }
    true
}

#[cfg(target_arch = "x86")]
fn faulting_address() -> usize {
    let value: usize;
    unsafe { core::arch::asm!("mov {}, cr2", out(reg) value, options(nomem, nostack)) };
    value
}

#[cfg(target_arch = "x86")]
unsafe fn context_of(regs: *mut c_void) -> GumCpuContext {
    let r = regs as *const u32;
    let mut c: GumCpuContext = unsafe { core::mem::zeroed() };
    unsafe {
        c.ebx = r.add(0).read();
        c.ecx = r.add(1).read();
        c.edx = r.add(2).read();
        c.esi = r.add(3).read();
        c.edi = r.add(4).read();
        c.ebp = r.add(5).read();
        c.eax = r.add(6).read();
        c.eip = r.add(12).read();
        c.esp = ((&raw mut RESUME_SCRATCH) as usize + RESUME_SCRATCH_SIZE - 16) as u32;
    }
    c
}

#[cfg(target_arch = "x86")]
#[unsafe(naked)]
unsafe extern "C" fn frida_i386_resume() {
    core::arch::naked_asm!(
        "mov esi, [edi + 8]",
        "mov ebp, [edi + 12]",
        "mov ebx, [edi + 20]",
        "mov edx, [edi + 24]",
        "mov ecx, [edi + 28]",
        "mov eax, [edi + 32]",
        "mov esp, [edi + 16]",
        "push dword ptr [edi]",
        "mov edi, [edi + 4]",
        "ret",
    );
}

#[cfg(target_arch = "x86")]
#[unsafe(no_mangle)]
unsafe extern "C" fn frida_cb_die(_nb: *mut NotifierBlock, action: c_long, data: *mut c_void) -> c_int {
    unsafe { dispatch(action, data) }
}

#[cfg(target_arch = "x86")]
unsafe extern "C" {
    fn frida_kcb_die(nb: *mut NotifierBlock, action: c_long, data: *mut c_void) -> c_int;
    fn frida_k_register_die_notifier(nb: *mut c_void) -> c_int;
    fn frida_k_unregister_die_notifier(nb: *mut c_void) -> c_int;
}

#[cfg(target_arch = "x86_64")]
pub fn install() {
    unsafe {
        (&raw mut REPORTER).write(NotifierBlock {
            notifier_call: Some(on_die),
            next: ptr::null_mut(),
            priority: 0,
        });
        _register_die_notifier(&raw mut REPORTER as *mut c_void);
    }
}

#[cfg(target_arch = "x86_64")]
pub fn uninstall() {
    unsafe { _unregister_die_notifier(&raw mut REPORTER as *mut c_void) };
}

#[cfg(target_arch = "x86_64")]
unsafe extern "C" fn on_die(_nb: *mut NotifierBlock, action: c_long, data: *mut c_void) -> c_int {
    unsafe { dispatch(action, data) }
}

#[cfg(target_arch = "x86_64")]
unsafe fn recovered(regs: *mut c_void) -> bool {
    let faulted_at = unsafe { program_counter(regs) };
    let (base, size) = crate::own_range();
    if faulted_at < base || faulted_at >= base + size {
        return false;
    }

    let mut context = unsafe { context_of(regs) };
    let handled = unsafe {
        gum_barebone_handle_exception(
            ACCESS_VIOLATION,
            faulted_at as *mut c_void,
            faulting_address() as *mut c_void,
            &mut context,
        )
    };
    if handled == 0 {
        return false;
    }

    unsafe { restore_context(regs, &context) };
    true
}

#[cfg(target_arch = "x86_64")]
fn faulting_address() -> usize {
    let value: usize;
    unsafe { core::arch::asm!("mov {}, cr2", out(reg) value, options(nomem, nostack)) };
    value
}

#[cfg(target_arch = "x86_64")]
unsafe fn program_counter(regs: *mut c_void) -> usize {
    unsafe { (regs as *const u64).add(16).read() as usize }
}

#[cfg(target_arch = "x86_64")]
unsafe fn context_of(regs: *mut c_void) -> GumCpuContext {
    let r = regs as *const u64;
    let mut c: GumCpuContext = unsafe { core::mem::zeroed() };
    unsafe {
        c.r15 = r.add(0).read();
        c.r14 = r.add(1).read();
        c.r13 = r.add(2).read();
        c.r12 = r.add(3).read();
        c.rbp = r.add(4).read();
        c.rbx = r.add(5).read();
        c.r11 = r.add(6).read();
        c.r10 = r.add(7).read();
        c.r9 = r.add(8).read();
        c.r8 = r.add(9).read();
        c.rax = r.add(10).read();
        c.rcx = r.add(11).read();
        c.rdx = r.add(12).read();
        c.rsi = r.add(13).read();
        c.rdi = r.add(14).read();
        c.rip = r.add(16).read();
        c.rsp = r.add(19).read();
    }
    c
}

#[cfg(target_arch = "x86_64")]
unsafe fn restore_context(regs: *mut c_void, c: &GumCpuContext) {
    let r = regs as *mut u64;
    unsafe {
        r.add(0).write(c.r15);
        r.add(1).write(c.r14);
        r.add(2).write(c.r13);
        r.add(3).write(c.r12);
        r.add(4).write(c.rbp);
        r.add(5).write(c.rbx);
        r.add(6).write(c.r11);
        r.add(7).write(c.r10);
        r.add(8).write(c.r9);
        r.add(9).write(c.r8);
        r.add(10).write(c.rax);
        r.add(11).write(c.rcx);
        r.add(12).write(c.rdx);
        r.add(13).write(c.rsi);
        r.add(14).write(c.rdi);
        r.add(16).write(c.rip);
        r.add(19).write(c.rsp);
    }
}

#[cfg(target_arch = "x86_64")]
unsafe extern "C" {
    static _register_die_notifier: unsafe extern "C" fn(*mut c_void) -> c_int;
    static _unregister_die_notifier: unsafe extern "C" fn(*mut c_void) -> c_int;
}

#[cfg(target_arch = "arm")]
const PROLOGUE: [u32; 2] = [0xe92d4010, 0xe52de004];

#[cfg(target_arch = "arm")]
static mut ORIGINAL_STUB: usize = 0;

#[cfg(target_arch = "arm")]
pub fn install() {
    let target = unsafe { _fixup_exception } as *const u32;
    for slot in 0..2 {
        if unsafe { target.add(slot).read_volatile() } != PROLOGUE[slot] {
            return;
        }
    }

    unsafe { build_original_stub(target) };
    if unsafe { ORIGINAL_STUB } == 0 {
        return;
    }

    unsafe {
        let addr = target as *mut u32;
        _patch_text(addr.add(1) as *mut c_void, frida_arm_fixup as usize as u32);
        _patch_text(addr as *mut c_void, 0xe51ff004);
    }
}

#[cfg(target_arch = "arm")]
pub fn uninstall() {
    let target = unsafe { _fixup_exception } as *mut u32;
    unsafe {
        _patch_text(target.add(1) as *mut c_void, PROLOGUE[1]);
        _patch_text(target as *mut c_void, PROLOGUE[0]);
    }
}

#[cfg(target_arch = "arm")]
unsafe fn build_original_stub(target: *const u32) {
    let stub = crate::kernel::alloc_code(16) as *mut u32;
    if stub.is_null() {
        return;
    }
    unsafe {
        stub.add(0).write(PROLOGUE[0]);
        stub.add(1).write(PROLOGUE[1]);
        stub.add(2).write(0xe51ff004);
        stub.add(3).write(target.add(2) as u32);
        ORIGINAL_STUB = stub as usize;
    }
}

#[cfg(target_arch = "arm")]
unsafe extern "C" fn frida_arm_fixup(regs: *mut c_void) -> c_int {
    if unsafe { recovered(regs) } {
        return 1;
    }
    let original: unsafe extern "C" fn(*mut c_void) -> c_int =
        unsafe { core::mem::transmute(ORIGINAL_STUB) };
    unsafe { original(regs) }
}

#[cfg(target_arch = "arm")]
unsafe fn recovered(regs: *mut c_void) -> bool {
    let faulted_at = unsafe { (regs as *const u32).add(15).read() as usize };
    let (base, size) = crate::own_range();
    if faulted_at < base || faulted_at >= base + size {
        return false;
    }

    let accessed: usize;
    unsafe { core::arch::asm!("mrc p15, 0, {}, c6, c0, 0", out(reg) accessed, options(nomem, nostack)) };

    let mut context = unsafe { context_of(regs) };
    let handled = unsafe {
        gum_barebone_handle_exception(
            ACCESS_VIOLATION,
            faulted_at as *mut c_void,
            accessed as *mut c_void,
            &mut context,
        )
    };
    if handled == 0 {
        return false;
    }

    unsafe { restore_context(regs, &context) };
    true
}

#[cfg(target_arch = "arm")]
unsafe fn context_of(regs: *mut c_void) -> GumCpuContext {
    let g = regs as *const u32;
    let mut r = [0u32; 8];
    for slot in 0..8 {
        r[slot] = unsafe { g.add(slot).read() };
    }
    let mut c: GumCpuContext = unsafe { core::mem::zeroed() };
    c.r = r;
    unsafe {
        c.r8 = g.add(8).read();
        c.r9 = g.add(9).read();
        c.r10 = g.add(10).read();
        c.r11 = g.add(11).read();
        c.r12 = g.add(12).read();
        c.sp = g.add(13).read();
        c.lr = g.add(14).read();
        c.pc = g.add(15).read();
        c.cpsr = g.add(16).read();
    }
    c
}

#[cfg(target_arch = "arm")]
unsafe fn restore_context(regs: *mut c_void, c: &GumCpuContext) {
    let g = regs as *mut u32;
    unsafe {
        for slot in 0..8 {
            g.add(slot).write(c.r[slot]);
        }
        g.add(8).write(c.r8);
        g.add(9).write(c.r9);
        g.add(10).write(c.r10);
        g.add(11).write(c.r11);
        g.add(12).write(c.r12);
        g.add(13).write(c.sp);
        g.add(14).write(c.lr);
        g.add(15).write(c.pc);
        g.add(16).write(c.cpsr);
    }
}

#[cfg(target_arch = "arm")]
unsafe extern "C" {
    static _fixup_exception: unsafe extern "C" fn();
    static _patch_text: unsafe extern "C" fn(*mut c_void, u32);
}

#[cfg(target_arch = "aarch64")]
const PROLOGUE: [u32; 4] = [0xd503233f, 0xa9be7bfd, 0x910003fd, 0xa90153f3];

#[cfg(target_arch = "aarch64")]
#[unsafe(no_mangle)]
static mut FRIDA_ARM64_ORIGINAL_CONT: u64 = 0;

#[cfg(target_arch = "aarch64")]
pub fn install() {
    let target = unsafe { _fixup_exception } as *const u32;
    for slot in 0..4 {
        if unsafe { target.add(slot).read_volatile() } != PROLOGUE[slot] {
            return;
        }
    }

    unsafe { FRIDA_ARM64_ORIGINAL_CONT = target.add(4) as u64 };

    let jump = detour_to(frida_arm64_fixup as usize);
    unsafe {
        let addr = target as *mut u32;
        _aarch64_insn_patch_text_nosync(addr.add(2) as *mut c_void, jump[2]);
        _aarch64_insn_patch_text_nosync(addr.add(3) as *mut c_void, jump[3]);
        _aarch64_insn_patch_text_nosync(addr.add(1) as *mut c_void, jump[1]);
        _aarch64_insn_patch_text_nosync(addr as *mut c_void, jump[0]);
    }
}

#[cfg(target_arch = "aarch64")]
pub fn uninstall() {
    let target = unsafe { _fixup_exception } as *mut u32;
    unsafe {
        for slot in 0..4 {
            _aarch64_insn_patch_text_nosync(target.add(slot) as *mut c_void, PROLOGUE[slot]);
        }
    }
}

#[cfg(target_arch = "aarch64")]
fn detour_to(destination: usize) -> [u32; 4] {
    [0x58000050, 0xd61f0200, destination as u32, (destination >> 32) as u32]
}

#[cfg(target_arch = "aarch64")]
core::arch::global_asm!(
    ".globl frida_arm64_call_original",
    "frida_arm64_call_original:",
    "paciasp",
    "stp x29, x30, [sp, #-32]!",
    "mov x29, sp",
    "stp x19, x20, [sp, #16]",
    "adrp x16, {cont}",
    "add x16, x16, :lo12:{cont}",
    "ldr x16, [x16]",
    "br x16",
    cont = sym FRIDA_ARM64_ORIGINAL_CONT,
);

#[cfg(target_arch = "aarch64")]
unsafe extern "C" {
    fn frida_arm64_call_original(regs: *mut c_void) -> c_int;
}

#[cfg(target_arch = "aarch64")]
unsafe extern "C" fn frida_arm64_fixup(regs: *mut c_void) -> c_int {
    if unsafe { recovered(regs) } {
        return 1;
    }
    unsafe { frida_arm64_call_original(regs) }
}

#[cfg(target_arch = "aarch64")]
unsafe fn recovered(regs: *mut c_void) -> bool {
    let faulted_at = unsafe { (regs as *const u64).add(32).read() as usize };
    let (base, size) = crate::own_range();
    if faulted_at < base || faulted_at >= base + size {
        return false;
    }

    let accessed: usize;
    unsafe { core::arch::asm!("mrs {}, far_el1", out(reg) accessed, options(nomem, nostack)) };

    let mut context = unsafe { context_of(regs) };
    let handled = unsafe {
        gum_barebone_handle_exception(
            ACCESS_VIOLATION,
            faulted_at as *mut c_void,
            accessed as *mut c_void,
            &mut context,
        )
    };
    if handled == 0 {
        let esr: u64;
        unsafe { core::arch::asm!("mrs {}, esr_el1", out(reg) esr, options(nomem, nostack)) };
        crate::note_unhandled_fault(esr, faulted_at as u64, accessed as u64);
        context.pc = frida_arm64_park as usize as u64;
        unsafe { restore_context(regs, &context) };
        return true;
    }

    unsafe { restore_context(regs, &context) };
    true
}

#[cfg(target_arch = "aarch64")]
extern "C" fn frida_arm64_park() -> ! {
    static mut PARK_TOKEN: u8 = 0;
    loop {
        crate::kernel::wait(core::ptr::addr_of!(PARK_TOKEN), Some(1_000_000), &mut || false);
    }
}

#[cfg(target_arch = "aarch64")]
unsafe fn context_of(regs: *mut c_void) -> GumCpuContext {
    let r = regs as *const u64;
    let mut x = [0u64; 29];
    for slot in 0..29 {
        x[slot] = unsafe { r.add(slot).read() };
    }
    GumCpuContext {
        pc: unsafe { r.add(32).read() },
        sp: unsafe { r.add(31).read() },
        nzcv: unsafe { r.add(33).read() },
        x,
        fp: unsafe { r.add(29).read() },
        lr: unsafe { r.add(30).read() },
    }
}

#[cfg(target_arch = "aarch64")]
unsafe fn restore_context(regs: *mut c_void, c: &GumCpuContext) {
    let r = regs as *mut u64;
    unsafe {
        for slot in 0..29 {
            r.add(slot).write(c.x[slot]);
        }
        r.add(29).write(c.fp);
        r.add(30).write(c.lr);
        r.add(31).write(c.sp);
        r.add(32).write(c.pc);
        r.add(33).write(c.nzcv);
    }
}

#[cfg(target_arch = "aarch64")]
unsafe extern "C" {
    static _fixup_exception: unsafe extern "C" fn();
    static _aarch64_insn_patch_text_nosync: unsafe extern "C" fn(*mut c_void, u32) -> c_int;
}

#[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "arm", target_arch = "aarch64")))]
pub fn install() {}

#[cfg(not(any(target_arch = "x86", target_arch = "x86_64", target_arch = "arm", target_arch = "aarch64")))]
pub fn uninstall() {}
