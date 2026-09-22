// Windows maps the page tables into themselves. Thus the entry for a page is at an address
// that you calculate from the page, and no kernel export is necessary. The two word sizes
// differ only in the address of the self-map and the number of levels.

pub fn protect(address: u64, size: usize, gum_prot: u32) -> bool {
    let mut at = address as usize & !(PAGE_SIZE - 1);
    let end = address as usize + size;

    while at < end {
        let span = reprotect(at, gum_prot);
        if span == 0 {
            at += PAGE_SIZE;
            continue;
        }

        invalidate_page(at);
        at = (at & !(span - 1)).wrapping_add(span);
    }

    true
}

// Read the page tables and join adjacent pages that have the same permissions. Report only
// the kernel half of the address space, because the other half belongs to the process that
// is current.
pub fn enumerate_ranges(found: &mut dyn FnMut(u64, u64, u32)) {
    let mut ranges = Ranges {
        found,
        base: 0,
        size: 0,
        protection: 0,
    };

    walk_kernel_space(&mut ranges);

    ranges.flush();
}

struct Ranges<'f> {
    found: &'f mut dyn FnMut(u64, u64, u32),
    base: usize,
    size: usize,
    protection: u32,
}

impl Ranges<'_> {
    fn add(&mut self, address: usize, size: usize, protection: u32) {
        if protection != self.protection || self.base.wrapping_add(self.size) != address {
            self.flush();
            self.base = address;
            self.protection = protection;
        }
        self.size += size;
    }

    fn flush(&mut self) {
        if self.protection != 0 {
            (self.found)(self.base as u64, self.size as u64, self.protection);
        }
        self.size = 0;
        self.protection = 0;
    }
}

#[cfg(target_arch = "x86")]
mod arch {
    use super::*;

    // A 32-bit kernel can use PAE. The two forms differ in the width of an entry, in the address
    // of the directory, and in the availability of the non-executable bit.
    pub fn protection_at(address: usize) -> u32 {
        unsafe {
            if pae_enabled() {
                let pde = ((PAE_PDE_BASE + (address >> 21) * 8) as *const u64).read_volatile();
                if (pde & PAGE_PRESENT as u64) == 0 {
                    return 0;
                }
                if (pde & PAGE_LARGE as u64) != 0 {
                    return protection_of(pde, PAGE_NO_EXECUTE);
                }
                let pte = ((PTE_BASE + (address >> 12) * 8) as *const u64).read_volatile();
                if (pte & PAGE_PRESENT as u64) == 0 {
                    return 0;
                }
                protection_of(pte, PAGE_NO_EXECUTE)
            } else {
                let pde = ((PDE_BASE + (address >> 22) * 4) as *const u32).read_volatile();
                if (pde & PAGE_PRESENT) == 0 {
                    return 0;
                }
                if (pde & PAGE_LARGE) != 0 {
                    return protection_of(pde as u64, 0);
                }
                let pte = ((PTE_BASE + (address >> 12) * 4) as *const u32).read_volatile();
                if (pte & PAGE_PRESENT) == 0 {
                    return 0;
                }
                protection_of(pte as u64, 0)
            }
        }
    }

    // Two gigabytes contain sufficiently few pages to examine each one.
    pub fn walk_kernel_space(ranges: &mut Ranges) {
        let mut address = KERNEL_SPACE_START;
        while address != 0 {
            let protection = protection_at(address);
            if protection != 0 {
                ranges.add(address, PAGE_SIZE, protection);
            }
            address = address.wrapping_add(PAGE_SIZE);
        }
    }

    // The kernel maps its pool with large pages, which have no page table. Thus the self-map has
    // no entry to change, and such a mapping is left alone, being already writable and executable.
    pub fn reprotect(address: usize, gum_prot: u32) -> usize {
        if !maps_small_page(address) {
            return 0;
        }

        unsafe {
            if pae_enabled() {
                let entry = (PTE_BASE + (address >> 12) * 8) as *mut u64;
                let value = apply_protection(entry.read_volatile(), gum_prot as u64,
                    PAGE_WRITEABLE as u64, PAGE_NO_EXECUTE);
                entry.write_volatile(value);
            } else {
                let entry = (PTE_BASE + (address >> 12) * 4) as *mut u32;
                let value = apply_protection(entry.read_volatile() as u64, gum_prot as u64,
                    PAGE_WRITEABLE as u64, 0);
                entry.write_volatile(value as u32);
            }
        }

        PAGE_SIZE
    }

    fn maps_small_page(address: usize) -> bool {
        unsafe {
            if pae_enabled() {
                let pde = ((PAE_PDE_BASE + (address >> 21) * 8) as *const u64).read_volatile();
                (pde & PAGE_PRESENT as u64) != 0 && (pde & PAGE_LARGE as u64) == 0
            } else {
                let pde = ((PDE_BASE + (address >> 22) * 4) as *const u32).read_volatile();
                (pde & PAGE_PRESENT) != 0 && (pde & PAGE_LARGE) == 0
            }
        }
    }

    fn pae_enabled() -> bool {
        let cr4: u32;
        unsafe {
            core::arch::asm!("mov {0:e}, cr4", out(reg) cr4,
                options(nomem, nostack, preserves_flags));
        }
        (cr4 & CR4_PAE) != 0
    }

    const KERNEL_SPACE_START: usize = 0x8000_0000;
    const PTE_BASE: usize = 0xc000_0000;
    const PDE_BASE: usize = 0xc030_0000;
    const PAE_PDE_BASE: usize = 0xc060_0000;
    const CR4_PAE: u32 = 1 << 5;
}

#[cfg(target_arch = "x86_64")]
mod arch {
    use super::*;

    pub fn protection_at(address: usize) -> u32 {
        for level in TOP_LEVEL..TABLE_LEVEL {
            let entry = entry_at(level, address);
            if (entry & PAGE_PRESENT as u64) == 0 {
                return 0;
            }
            if (entry & PAGE_LARGE as u64) != 0 {
                return protection_of(entry, PAGE_NO_EXECUTE);
            }
        }

        let entry = entry_at(TABLE_LEVEL, address);
        if (entry & PAGE_PRESENT as u64) == 0 {
            return 0;
        }

        protection_of(entry, PAGE_NO_EXECUTE)
    }

    // Half of a 48-bit space contains too many pages to examine each one. Thus the walk goes down
    // the levels, and an absent table costs nothing.
    pub fn walk_kernel_space(ranges: &mut Ranges) {
        for index in ENTRIES_PER_TABLE / 2..ENTRIES_PER_TABLE {
            descend(sign_extend(index << LEVEL_SHIFTS[TOP_LEVEL]), TOP_LEVEL, ranges);
        }
    }

    fn descend(address: usize, level: usize, ranges: &mut Ranges) {
        let entry = entry_at(level, address);
        if (entry & PAGE_PRESENT as u64) == 0 {
            return;
        }

        let span = 1usize << LEVEL_SHIFTS[level];
        if level == TABLE_LEVEL || (entry & PAGE_LARGE as u64) != 0 {
            ranges.add(address, span, protection_of(entry, PAGE_NO_EXECUTE));
            return;
        }

        let child_span = span / ENTRIES_PER_TABLE;
        for index in 0..ENTRIES_PER_TABLE {
            descend(address.wrapping_add(index * child_span), level + 1, ranges);
        }
    }

    // The kernel maps its pool with large pages, which have no page table. Thus the self-map has
    // no entry to change, and such a mapping is left alone, being already writable and executable.
    pub fn reprotect(address: usize, gum_prot: u32) -> usize {
        if !maps_small_page(address) {
            return 0;
        }

        let entry = entry_pointer(TABLE_LEVEL, address);
        unsafe {
            let value = apply_protection(entry.read_volatile(), gum_prot as u64,
                PAGE_WRITEABLE as u64, PAGE_NO_EXECUTE);
            entry.write_volatile(value);
        }

        PAGE_SIZE
    }

    fn maps_small_page(address: usize) -> bool {
        for level in TOP_LEVEL..TABLE_LEVEL {
            let entry = entry_at(level, address);
            if (entry & PAGE_PRESENT as u64) == 0 || (entry & PAGE_LARGE as u64) != 0 {
                return false;
            }
        }

        true
    }

    fn entry_at(level: usize, address: usize) -> u64 {
        unsafe { entry_pointer(level, address).read_volatile() }
    }

    fn entry_pointer(level: usize, address: usize) -> *mut u64 {
        let index = (address >> LEVEL_SHIFTS[level]) & LEVEL_INDEX_MASKS[level];
        (LEVEL_BASES[level] + index * 8) as *mut u64
    }

    fn sign_extend(address: usize) -> usize {
        (((address << CANONICAL_SPARE_BITS) as isize) >> CANONICAL_SPARE_BITS) as usize
    }

    const TOP_LEVEL: usize = 0;
    const TABLE_LEVEL: usize = 3;
    const ENTRIES_PER_TABLE: usize = 512;
    const CANONICAL_SPARE_BITS: usize = 16;

    const LEVEL_SHIFTS: [usize; 4] = [39, 30, 21, 12];
    const LEVEL_INDEX_MASKS: [usize; 4] = [0x1ff, 0x3_ffff, 0x7ff_ffff, 0xf_ffff_ffff];
    const LEVEL_BASES: [usize; 4] = [
        0xffff_f6fb_7dbe_d000,
        0xffff_f6fb_7da0_0000,
        0xffff_f6fb_4000_0000,
        0xffff_f680_0000_0000,
    ];
}

#[cfg(target_arch = "aarch64")]
mod arch {
    use super::*;

    pub fn protection_at(address: usize) -> u32 {
        match resolve(address) {
            Some((descriptor, _)) =>
                protection_of_descriptor(unsafe { descriptor.read_volatile() }, address),
            None => 0,
        }
    }

    pub fn walk_kernel_space(ranges: &mut Ranges) {
        let space = kernel_space();
        for index in 0..space.entries {
            descend(space.base | (index << LEVEL_SHIFTS[TOP_LEVEL]), TOP_LEVEL, table_base(),
                ranges);
        }
    }

    fn descend(address: usize, level: usize, table: u64, ranges: &mut Ranges) {
        let descriptor = read_descriptor(table, level, address);
        if !describes_memory(descriptor) {
            return;
        }

        let span = 1usize << LEVEL_SHIFTS[level];
        if level == TABLE_LEVEL || describes_block(descriptor) {
            ranges.add(address, span, protection_of_descriptor(descriptor, address));
            return;
        }

        let child_span = span / ENTRIES_PER_TABLE;
        for index in 0..ENTRIES_PER_TABLE {
            descend(address.wrapping_add(index * child_span), level + 1,
                output_address(descriptor), ranges);
        }
    }

    pub fn reprotect(address: usize, gum_prot: u32) -> usize {
        let Some((descriptor, level)) = resolve(address) else {
            return 0;
        };

        unsafe {
            let value =
                apply_protection_to_descriptor(descriptor.read_volatile(), gum_prot, address);
            descriptor.write_volatile(value);
        }

        1usize << LEVEL_SHIFTS[level]
    }

    fn resolve(address: usize) -> Option<(*mut u64, usize)> {
        let mut table = table_base_for(address);
        for level in TOP_LEVEL..=TABLE_LEVEL {
            let at = descriptor_pointer_in(table, level, address);
            let descriptor = unsafe { at.read_volatile() };
            if !describes_memory(descriptor) {
                return None;
            }
            if level == TABLE_LEVEL || describes_block(descriptor) {
                return Some((at, level));
            }
            table = output_address(descriptor);
        }
        None
    }

    fn read_descriptor(table: u64, level: usize, address: usize) -> u64 {
        unsafe { descriptor_pointer_in(table, level, address).read_volatile() }
    }

    fn descriptor_pointer_in(table: u64, level: usize, address: usize) -> *mut u64 {
        let index = (address >> LEVEL_SHIFTS[level]) & index_mask(level);
        unsafe { crate::winnt::virtual_for_physical(table).cast::<u64>().add(index) }
    }

    fn index_mask(level: usize) -> usize {
        if level == TOP_LEVEL {
            kernel_space().entries - 1
        } else {
            ENTRIES_PER_TABLE - 1
        }
    }

    fn describes_memory(descriptor: u64) -> bool {
        (descriptor & DESCRIPTOR_VALID) != 0
    }

    fn describes_block(descriptor: u64) -> bool {
        (descriptor & DESCRIPTOR_TABLE) == 0
    }

    fn output_address(descriptor: u64) -> u64 {
        descriptor & OUTPUT_ADDRESS_MASK
    }

    fn protection_of_descriptor(descriptor: u64, address: usize) -> u32 {
        let mut prot = GUM_PAGE_READ;
        if (descriptor & DESCRIPTOR_READ_ONLY) == 0 {
            prot |= GUM_PAGE_WRITE;
        }
        if (descriptor & no_execute_for(address)) == 0 {
            prot |= GUM_PAGE_EXECUTE;
        }
        prot
    }

    fn apply_protection_to_descriptor(descriptor: u64, gum_prot: u32, address: usize) -> u64 {
        let mut value = descriptor;

        if (gum_prot & GUM_PAGE_WRITE) != 0 {
            value &= !DESCRIPTOR_READ_ONLY;
        } else {
            value |= DESCRIPTOR_READ_ONLY;
        }

        if (gum_prot & GUM_PAGE_EXECUTE) != 0 {
            value &= !no_execute_for(address);
        } else {
            value |= no_execute_for(address);
        }

        value
    }

    fn table_base() -> u64 {
        read_system_register!("ttbr1_el1") & TABLE_BASE_MASK
    }

    fn no_execute_for(address: usize) -> u64 {
        if is_in_lower_half(address) {
            DESCRIPTOR_NEVER_EXECUTE_AT_EL0
        } else {
            DESCRIPTOR_NEVER_EXECUTE_AT_EL1
        }
    }

    fn table_base_for(address: usize) -> u64 {
        if is_in_lower_half(address) {
            read_system_register!("ttbr0_el1") & TABLE_BASE_MASK
        } else {
            table_base()
        }
    }

    fn is_in_lower_half(address: usize) -> bool {
        (address as u64) < LOWER_HALF_LIMIT
    }

    const LOWER_HALF_LIMIT: u64 = 1 << 48;

    fn kernel_space() -> KernelSpace {
        let address_bits = 64 - ((read_system_register!("tcr_el1") >> TCR_T1SZ_SHIFT)
            & TCR_SIZE_MASK) as usize;

        KernelSpace {
            base: !((1usize << address_bits) - 1),
            entries: 1usize << (address_bits - LEVEL_SHIFTS[TOP_LEVEL]),
        }
    }

    struct KernelSpace {
        base: usize,
        entries: usize,
    }

    const TOP_LEVEL: usize = 0;
    const TABLE_LEVEL: usize = 3;
    const ENTRIES_PER_TABLE: usize = 512;

    const LEVEL_SHIFTS: [usize; 4] = [39, 30, 21, 12];

    const DESCRIPTOR_VALID: u64 = 1 << 0;
    const DESCRIPTOR_TABLE: u64 = 1 << 1;
    const DESCRIPTOR_READ_ONLY: u64 = 1 << 7;
    const DESCRIPTOR_NEVER_EXECUTE_AT_EL1: u64 = 1 << 53;
    const DESCRIPTOR_NEVER_EXECUTE_AT_EL0: u64 = 1 << 54;
    const OUTPUT_ADDRESS_MASK: u64 = 0x0000_ffff_ffff_f000;
    const TABLE_BASE_MASK: u64 = 0x0000_ffff_ffff_fffe;
    const TCR_T1SZ_SHIFT: u64 = 16;
    const TCR_SIZE_MASK: u64 = 0x3f;
}

#[cfg(target_arch = "aarch64")]
macro_rules! read_system_register {
    ($name:literal) => {{
        let value: u64;
        unsafe {
            core::arch::asm!(concat!("mrs {0}, ", $name), out(reg) value,
                options(nomem, nostack, preserves_flags));
        }
        value
    }};
}

#[cfg(target_arch = "aarch64")]
use read_system_register;

pub use arch::protection_at;

use arch::{reprotect, walk_kernel_space};

fn protection_of(entry: u64, no_execute: u64) -> u32 {
    let mut prot = GUM_PAGE_READ;
    if (entry & PAGE_WRITEABLE as u64) != 0 {
        prot |= GUM_PAGE_WRITE;
    }
    if no_execute == 0 || (entry & no_execute) == 0 {
        prot |= GUM_PAGE_EXECUTE;
    }
    prot
}

fn apply_protection(entry: u64, gum_prot: u64, writeable: u64, no_execute: u64) -> u64 {
    let mut value = entry;

    if (gum_prot & GUM_PAGE_WRITE as u64) != 0 {
        value |= writeable;
    } else {
        value &= !writeable;
    }

    if no_execute != 0 {
        if (gum_prot & GUM_PAGE_EXECUTE as u64) != 0 {
            value &= !no_execute;
        } else {
            value |= no_execute;
        }
    }

    value
}

#[cfg(any(target_arch = "x86", target_arch = "x86_64"))]
fn invalidate_page(address: usize) {
    unsafe {
        core::arch::asm!("invlpg [{0}]", in(reg) address, options(nostack, preserves_flags));
    }
}

#[cfg(target_arch = "aarch64")]
fn invalidate_page(address: usize) {
    unsafe {
        core::arch::asm!(
            "dsb ishst",
            "tlbi vaae1is, {0}",
            "dsb ish",
            "isb",
            in(reg) (address >> 12) as u64,
            options(nostack, preserves_flags));
    }
}

const PAGE_SIZE: usize = 4096;
const PAGE_PRESENT: u32 = 0x1;
const PAGE_WRITEABLE: u32 = 0x2;
const PAGE_LARGE: u32 = 0x80;
const PAGE_NO_EXECUTE: u64 = 1 << 63;

pub(crate) const GUM_PAGE_READ: u32 = 0x1;
pub(crate) const GUM_PAGE_WRITE: u32 = 0x2;
pub(crate) const GUM_PAGE_EXECUTE: u32 = 0x4;
