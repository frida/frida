use alloc::boxed::Box;
use alloc::string::String;
use alloc::vec::Vec;
use core::ffi::{c_char, c_int, c_long, c_void};
use core::ptr;

use super::layout::{field_offset, struct_size};
use crate::bindings::{g_free, gsize, gum_memory_read};
use super::{LoadedModule, ModuleEvent};

pub fn enumerate_modules() -> Vec<LoadedModule> {
    let Some(layout) = module_layout() else {
        return Vec::new();
    };

    let head = unsafe { _modules } as usize;
    if head == 0 {
        return Vec::new();
    }

    let mut modules = Vec::new();
    let mut node = word_at(head);
    let mut left = MAX_MODULES;
    while node != head && node != 0 && left != 0 {
        let module = node - layout.list;
        if is_live(module, &layout) {
            modules.push(describe(module, &layout));
        }

        node = word_at(node);
        left -= 1;
    }

    modules
}

pub fn enumerate_module_symbols(
    base: u64,
    on_symbol: &mut dyn FnMut(*const c_char, u64, u64, u8, bool) -> bool,
) -> bool {
    let Some(layout) = module_layout() else {
        return false;
    };
    let Some(kallsyms) = layout.kallsyms.as_ref() else {
        return false;
    };
    let Some(module) = module_at(base, layout) else {
        return false;
    };

    let table = word_at(module + kallsyms.at);
    if table == 0 {
        return false;
    }

    let symbols = word_at(table + kallsyms.symtab);
    let names = word_at(table + kallsyms.strtab);
    let count = u32_at(table + kallsyms.num_symtab) as usize;
    if symbols == 0 || names == 0 {
        return false;
    }

    for index in 0..count {
        let symbol = symbols + index * SYMBOL_SIZE;
        let value = symbol_value(symbol);
        if value == 0 {
            continue;
        }

        let info = symbol_info(symbol);
        let name = (names + u32_at(symbol) as usize) as *const c_char;
        if !on_symbol(
            name,
            value as u64,
            symbol_size(symbol),
            info & 0xf,
            info >> 4 != LOCAL,
        ) {
            break;
        }
    }

    true
}

pub fn enumerate_module_exports(
    base: u64,
    on_export: &mut dyn FnMut(*const c_char, u64) -> bool,
) -> bool {
    let Some(layout) = module_layout() else {
        return false;
    };
    let Some(exports) = layout.exports.as_ref() else {
        return false;
    };
    let Some(module) = module_at(base, layout) else {
        return false;
    };

    let table = word_at(module + exports.at);
    let count = u32_at(module + exports.count) as usize;
    if table == 0 {
        return true;
    }

    for index in 0..count {
        let entry = table + index * exports.stride;
        let (name, address) = if exports.relative {
            (
                relative_target(entry + OFFSET_SIZE),
                relative_target(entry) as u64,
            )
        } else {
            (word_at(entry + WORD), word_at(entry) as u64)
        };
        if name == 0 || address == 0 {
            continue;
        }

        if !on_export(name as *const c_char, address) {
            break;
        }
    }

    true
}

fn module_at(base: u64, layout: &Layout) -> Option<usize> {
    let head = unsafe { _modules } as usize;
    if head == 0 {
        return None;
    }

    let mut node = word_at(head);
    let mut left = MAX_MODULES;
    while node != head && node != 0 && left != 0 {
        let module = node - layout.list;
        if describe(module, layout).base == base {
            return Some(module);
        }

        node = word_at(node);
        left -= 1;
    }

    None
}

fn relative_target(at: usize) -> usize {
    let offset = unsafe { (at as *const i32).read_volatile() };

    at.wrapping_add_signed(offset as isize)
}

pub fn watch_modules(on_event: fn(ModuleEvent, LoadedModule)) {
    if module_layout().is_none() {
        return;
    }

    unsafe {
        if _register_module_notifier.is_none() {
            return;
        }

        WATCHER = Some(on_event);
        NOTIFIER = Box::into_raw(Box::new(NotifierBlock {
            notifier_call: Some(NOTIFIER_CALL),
            next: ptr::null_mut(),
            priority: 0,
        }));

        register_notifier(NOTIFIER as *mut c_void);
    }
}

pub fn unwatch_modules() {
    unsafe {
        if (&raw const WATCHER).read().is_none() {
            return;
        }

        if _unregister_module_notifier.is_some() {
            unregister_notifier(NOTIFIER as *mut c_void);
        }

        drop(Box::from_raw(NOTIFIER));
        NOTIFIER = ptr::null_mut();
        WATCHER = None;
    }
}

#[cfg(not(target_arch = "x86"))]
unsafe fn register_notifier(block: *mut c_void) {
    unsafe {
        if let Some(register) = _register_module_notifier {
            register(block);
        }
    }
}

#[cfg(not(target_arch = "x86"))]
unsafe fn unregister_notifier(block: *mut c_void) {
    unsafe {
        if let Some(unregister) = _unregister_module_notifier {
            unregister(block);
        }
    }
}

#[cfg(target_arch = "x86")]
unsafe fn register_notifier(block: *mut c_void) {
    unsafe { frida_k_register_module_notifier(block) };
}

#[cfg(target_arch = "x86")]
unsafe fn unregister_notifier(block: *mut c_void) {
    unsafe { frida_k_unregister_module_notifier(block) };
}

#[cfg(not(target_arch = "x86"))]
const NOTIFIER_CALL: unsafe extern "C" fn(*mut NotifierBlock, c_long, *mut c_void) -> c_int =
    on_module_state;

#[cfg(target_arch = "x86")]
const NOTIFIER_CALL: unsafe extern "C" fn(*mut NotifierBlock, c_long, *mut c_void) -> c_int =
    frida_kcb_module_state;

#[cfg(target_arch = "x86")]
#[unsafe(no_mangle)]
unsafe extern "C" fn frida_cb_module_state(
    block: *mut NotifierBlock,
    action: c_long,
    module: *mut c_void,
) -> c_int {
    unsafe { on_module_state(block, action, module) }
}

unsafe extern "C" fn on_module_state(
    _block: *mut NotifierBlock,
    action: c_long,
    module: *mut c_void,
) -> c_int {
    let kind = match action {
        MODULE_STATE_LIVE => ModuleEvent::Loaded,
        MODULE_STATE_GOING => ModuleEvent::Unloaded,
        _ => return NOTIFY_DONE,
    };

    let Some(on_event) = (unsafe { WATCHER }) else {
        return NOTIFY_DONE;
    };
    let Some(layout) = module_layout() else {
        return NOTIFY_DONE;
    };

    on_event(kind, describe(module as usize, &layout));

    NOTIFY_DONE
}

fn describe(module: usize, layout: &Layout) -> LoadedModule {
    let (base, size) = match layout.memory {
        Memory::Described { at, base, size } => (word_at(module + at + base), word_at(module + at + size)),
        Memory::Split { base, size } => (word_at(module + base), word_at(module + size)),
    };

    LoadedModule {
        name: text_at(module + layout.name),
        version: layout
            .version
            .map(|at| text_at(word_at(module + at)))
            .unwrap_or_default(),
        base: base as u64,
        size: (size & 0xffff_ffff) as u64,
    }
}

fn is_live(module: usize, layout: &Layout) -> bool {
    let Some(at) = layout.state else {
        return true;
    };

    let state = unsafe { ((module + at) as *const u32).read_volatile() };

    state == MODULE_STATE_LIVE as u32
}

fn module_layout() -> Option<&'static Layout> {
    unsafe {
        let known = (&raw mut LAYOUT).as_mut().unwrap();
        if known.is_none() {
            *known = discover_layout();
        }
        known.as_ref()
    }
}

static mut LAYOUT: Option<Layout> = None;

struct Layout {
    list: usize,
    name: usize,
    version: Option<usize>,
    state: Option<usize>,
    memory: Memory,
    kallsyms: Option<Kallsyms>,
    exports: Option<Exports>,
}

struct Kallsyms {
    at: usize,
    symtab: usize,
    num_symtab: usize,
    strtab: usize,
}

struct Exports {
    at: usize,
    count: usize,
    stride: usize,
    relative: bool,
}

enum Memory {
    Described { at: usize, base: usize, size: usize },
    Split { base: usize, size: usize },
}

fn discover_layout() -> Option<Layout> {
    layout_from_types().or_else(layout_from_probing)
}

fn layout_from_types() -> Option<Layout> {
    let memory = match field_offset("module", "mem") {
        Some(at) => Memory::Described {
            at,
            base: field_offset("module_memory", "base")?,
            size: field_offset("module_memory", "size")?,
        },
        None => match field_offset("module", "core_layout") {
            Some(at) => Memory::Described {
                at,
                base: field_offset("module_layout", "base")?,
                size: field_offset("module_layout", "size")?,
            },
            None => Memory::Split {
                base: field_offset("module", "module_core")?,
                size: field_offset("module", "core_size")?,
            },
        },
    };

    let list = field_offset("module", "list")?;

    Some(Layout {
        list,
        name: field_offset("module", "name")?,
        version: field_offset("module", "version"),
        state: field_offset("module", "state"),
        memory,
        kallsyms: kallsyms_from_types(),
        exports: exports_from_types(list),
    })
}

fn layout_from_probing() -> Option<Layout> {
    let list = WORD;
    let name = list + 2 * WORD;
    let start = (name + MODULE_NAME_LEN + WORD - 1) & !(WORD - 1);
    let (at, size) = probe_memory(list, start)?;
    let memory = Memory::Described { at, base: 0, size };

    Some(Layout {
        list,
        name,
        version: None,
        state: Some(0),
        kallsyms: probe_kallsyms(list, start, &memory),
        exports: probe_exports(list, start, &memory),
        memory,
    })
}

fn probe_memory(list: usize, start: usize) -> Option<(usize, usize)> {
    let head = unsafe { _modules } as usize;
    if head == 0 {
        return None;
    }

    for at in (start..PROBE_SPAN).step_by(WORD) {
        for size in (WORD..=MAX_SIZE_DISTANCE).step_by(WORD) {
            if each_module(head, list, |module| describes_memory(module, at, size)) {
                return Some((at, size));
            }
        }
    }

    None
}

fn describes_memory(module: usize, at: usize, size: usize) -> bool {
    let base = word_at(module + at);
    let size = unsafe { ((module + at + size) as *const u32).read_volatile() } as usize;

    base != 0
        && base & (PAGE_SIZE - 1) == 0
        && size != 0
        && size & (PAGE_SIZE - 1) == 0
        && size <= MAX_MODULE_SIZE
        && base.abs_diff(module) <= MODULE_REACH
}

fn each_module(head: usize, list: usize, is_sane: impl Fn(usize) -> bool) -> bool {
    let mut node = word_at(head);
    let mut seen = 0;
    let mut left = MAX_MODULES;
    while node != head && node != 0 && left != 0 {
        if !is_sane(node - list) {
            return false;
        }
        seen += 1;

        node = word_at(node);
        left -= 1;
    }

    seen != 0
}

fn kallsyms_from_types() -> Option<Kallsyms> {
    Some(Kallsyms {
        at: field_offset("module", "kallsyms")?,
        symtab: field_offset("mod_kallsyms", "symtab")?,
        num_symtab: field_offset("mod_kallsyms", "num_symtab")?,
        strtab: field_offset("mod_kallsyms", "strtab")?,
    })
}

fn exports_from_types(_list: usize) -> Option<Exports> {
    let at = field_offset("module", "syms")?;
    let count = field_offset("module", "num_syms")?;
    let relative = field_offset("kernel_symbol", "value_offset").is_some();

    Some(Exports {
        at,
        count,
        stride: struct_size("kernel_symbol").unwrap_or(stride_of_exports(relative)),
        relative,
    })
}

fn probe_kallsyms(list: usize, start: usize, memory: &Memory) -> Option<Kallsyms> {
    let head = unsafe { _modules } as usize;

    for at in (start..PROBE_SPAN).step_by(WORD) {
        if each_module(head, list, |module| describes_kallsyms(module, at, memory)) {
            return Some(Kallsyms {
                at,
                symtab: 0,
                num_symtab: WORD,
                strtab: 2 * WORD,
            });
        }
    }

    None
}

fn describes_kallsyms(module: usize, at: usize, memory: &Memory) -> bool {
    let owned = regions_of(module, memory);

    let table = word_at(module + at);
    if !owned.holds(table, 3 * WORD) {
        return false;
    }

    let Some(symbols) = try_read_word(table) else {
        return false;
    };
    let Some(count) = try_read_u32(table + WORD) else {
        return false;
    };
    let Some(names) = try_read_word(table + 2 * WORD) else {
        return false;
    };

    let count = count as usize;
    if count == 0 || count > MAX_SYMBOLS {
        return false;
    }
    if !owned.holds(symbols, count * SYMBOL_SIZE) || !owned.holds(names, 1) {
        return false;
    }

    try_read_byte(names) == Some(0)
}

fn probe_exports(list: usize, start: usize, memory: &Memory) -> Option<Exports> {
    let head = unsafe { _modules } as usize;

    for at in (start..PROBE_SPAN).step_by(WORD) {
        for count in counts_near(at, start) {
            for relative in [true, false] {
                let stride = stride_of_exports(relative);
                if exports_shaped(head, list, at, count, stride, relative, memory) {
                    return Some(Exports {
                        at,
                        count,
                        stride,
                        relative,
                    });
                }
            }
        }
    }

    None
}

fn exports_shaped(
    head: usize,
    list: usize,
    at: usize,
    count: usize,
    stride: usize,
    relative: bool,
    memory: &Memory,
) -> bool {
    let mut evidence = false;
    let mut node = word_at(head);
    let mut left = MAX_MODULES;
    while node != head && node != 0 && left != 0 {
        let module = node - list;
        let table = word_at(module + at);
        let exported = u32_at(module + count) as usize;

        if table == 0 || exported == 0 {
            if table != 0 || exported != 0 {
                return false;
            }
        } else {
            if exported > MAX_EXPORTS {
                return false;
            }

            let owned = regions_of(module, memory);
            if !owned.holds(table, exported * stride) {
                return false;
            }

            for index in 0..exported.min(EXPORTS_SAMPLED) {
                if !names_an_export(table + index * stride, relative, &owned) {
                    return false;
                }
            }

            evidence = true;
        }

        node = word_at(node);
        left -= 1;
    }

    evidence
}

fn names_an_export(entry: usize, relative: bool, owned: &Regions) -> bool {
    let name = if relative {
        let Some(offset) = try_read_u32(entry + OFFSET_SIZE) else {
            return false;
        };
        (entry + OFFSET_SIZE).wrapping_add_signed(offset as i32 as isize)
    } else {
        match try_read_word(entry + WORD) {
            Some(name) => name,
            None => return false,
        }
    };
    if !owned.holds(name, 1) {
        return false;
    }

    match try_read_byte(name) {
        Some(first) => first.is_ascii_alphabetic() || first == b'_',
        None => false,
    }
}

fn counts_near(at: usize, start: usize) -> impl Iterator<Item = usize> {
    let after = (WORD..=MAX_COUNT_DISTANCE).step_by(WORD).map(move |d| at + d);
    let before = (WORD..=MAX_COUNT_DISTANCE)
        .step_by(WORD)
        .filter_map(move |d| at.checked_sub(d).filter(|&count| count >= start));

    after.chain(before)
}

fn stride_of_exports(relative: bool) -> usize {
    if relative { 3 * OFFSET_SIZE } else { 3 * WORD }
}

fn regions_of(module: usize, memory: &Memory) -> Regions {
    let Memory::Described { at, base, size } = *memory else {
        return Regions::default();
    };

    let mut owned = Regions::default();
    for offset in (0..REGION_WINDOW).step_by(WORD) {
        let entry = module + at + offset;
        let start = word_at(entry + base);
        let span = u32_at(entry + size) as usize;

        if start != 0
            && start & (PAGE_SIZE - 1) == 0
            && span != 0
            && span & (PAGE_SIZE - 1) == 0
            && span <= MAX_MODULE_SIZE
            && start.abs_diff(module) <= MODULE_REACH
        {
            owned.add(start, span);
        }
    }

    owned
}

#[derive(Default)]
struct Regions {
    entries: [(usize, usize); MAX_REGIONS],
    count: usize,
}

impl Regions {
    fn add(&mut self, base: usize, size: usize) {
        if self.count == MAX_REGIONS {
            return;
        }

        self.entries[self.count] = (base, size);
        self.count += 1;
    }

    fn holds(&self, address: usize, span: usize) -> bool {
        let Some(end) = address.checked_add(span) else {
            return false;
        };

        self.entries[..self.count]
            .iter()
            .any(|&(base, size)| address >= base && end <= base + size)
    }
}

fn reads_as_pointer(address: usize) -> bool {
    address >= LOWEST_MAPPING && address & (OFFSET_SIZE - 1) == 0
}

fn try_read_word(address: usize) -> Option<usize> {
    Some(usize::from_ne_bytes(try_read(address)?))
}

fn try_read_u32(address: usize) -> Option<u32> {
    let bytes = try_read::<4>(address)?;
    Some(u32::from_ne_bytes(bytes))
}

fn try_read_byte(address: usize) -> Option<u8> {
    Some(try_read::<1>(address)?[0])
}

fn try_read<const N: usize>(address: usize) -> Option<[u8; N]> {
    unsafe {
        let mut read: gsize = 0;
        let data = gum_memory_read(address as *const c_void, N as gsize, &mut read);
        if data.is_null() {
            return None;
        }

        let mut bytes = [0u8; N];
        core::ptr::copy_nonoverlapping(data as *const u8, bytes.as_mut_ptr(), N);
        g_free(data as *mut c_void);

        Some(bytes)
    }
}

fn word_at(address: usize) -> usize {
    unsafe { (address as *const usize).read_volatile() }
}

fn u32_at(address: usize) -> u32 {
    unsafe { (address as *const u32).read_volatile() }
}

#[cfg(target_pointer_width = "64")]
fn symbol_value(symbol: usize) -> usize {
    word_at(symbol + 8)
}

#[cfg(target_pointer_width = "64")]
fn symbol_size(symbol: usize) -> u64 {
    word_at(symbol + 16) as u64
}

#[cfg(target_pointer_width = "64")]
fn symbol_info(symbol: usize) -> u8 {
    unsafe { ((symbol + 4) as *const u8).read_volatile() }
}

#[cfg(target_pointer_width = "32")]
fn symbol_value(symbol: usize) -> usize {
    word_at(symbol + 4)
}

#[cfg(target_pointer_width = "32")]
fn symbol_size(symbol: usize) -> u64 {
    word_at(symbol + 8) as u64
}

#[cfg(target_pointer_width = "32")]
fn symbol_info(symbol: usize) -> u8 {
    unsafe { ((symbol + 12) as *const u8).read_volatile() }
}

fn text_at(address: usize) -> String {
    if address == 0 {
        return String::new();
    }

    let mut text = String::new();
    for i in 0..MAX_NAME {
        let byte = unsafe { ((address + i) as *const u8).read_volatile() };
        if byte == 0 {
            break;
        }
        text.push(byte as char);
    }

    text
}

static mut WATCHER: Option<fn(ModuleEvent, LoadedModule)> = None;

static mut NOTIFIER: *mut NotifierBlock = ptr::null_mut();

#[repr(C)]
struct NotifierBlock {
    notifier_call:
        Option<unsafe extern "C" fn(*mut NotifierBlock, c_long, *mut c_void) -> c_int>,
    next: *mut NotifierBlock,
    priority: c_int,
}

const MODULE_STATE_LIVE: c_long = 0;
const MODULE_STATE_GOING: c_long = 2;
const NOTIFY_DONE: c_int = 0;
const MAX_MODULES: usize = 4096;
const WORD: usize = core::mem::size_of::<usize>();
const MODULE_NAME_LEN: usize = 64 - WORD;
const PAGE_SIZE: usize = 4096;
const PROBE_SPAN: usize = 1024;
const MAX_MODULE_SIZE: usize = 256 * 1024 * 1024;
const MODULE_REACH: usize = 512 * 1024 * 1024;
const MAX_SIZE_DISTANCE: usize = 3 * WORD;
const MAX_COUNT_DISTANCE: usize = 3 * WORD;
const REGION_WINDOW: usize = 512;
const MAX_REGIONS: usize = 16;
const OFFSET_SIZE: usize = 4;
const SYMBOL_SIZE: usize = if WORD == 8 { 24 } else { 16 };
const LOWEST_MAPPING: usize = 1 << 20;
const MAX_SYMBOLS: usize = 1 << 20;
const MAX_EXPORTS: usize = 1 << 16;
const EXPORTS_SAMPLED: usize = 64;
const LOCAL: u8 = 0;
const MAX_NAME: usize = 64;

#[cfg(target_arch = "x86")]
unsafe extern "C" {
    fn frida_kcb_module_state(
        block: *mut NotifierBlock,
        action: c_long,
        module: *mut c_void,
    ) -> c_int;
    fn frida_k_register_module_notifier(block: *mut c_void) -> c_int;
    fn frida_k_unregister_module_notifier(block: *mut c_void) -> c_int;
}

unsafe extern "C" {
    static _modules: *const c_void;
    static _register_module_notifier: Option<unsafe extern "C" fn(*mut c_void) -> c_int>;
    #[allow(dead_code)]
    static _unregister_module_notifier: Option<unsafe extern "C" fn(*mut c_void) -> c_int>;
}
