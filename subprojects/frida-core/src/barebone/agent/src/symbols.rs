use crate::bindings::{
    GPatternSpec, g_pattern_spec_free, g_pattern_spec_match_string, g_pattern_spec_new,
};
use crate::kernel::get_kernel_base;
use alloc::ffi::CString;
use core::cmp::Ordering;
use core::ffi::{CStr, c_char};
use core::mem::size_of;
use core::ptr;

pub struct SymbolTable {
    data: &'static [u8],
    symbol_count: usize,
}

impl SymbolTable {
    pub const fn empty() -> Self {
        Self {
            data: &[],
            symbol_count: 0,
        }
    }

    pub fn new(data: &'static [u8]) -> Self {
        if data.len() < 4 {
            return Self::empty();
        }
        let symbol_count = unsafe { *(data.as_ptr() as *const u32) as usize };
        Self { data, symbol_count }
    }

    pub fn is_empty(&self) -> bool {
        self.symbol_count == 0
    }

    pub fn symbol_count(&self) -> usize {
        self.symbol_count
    }

    pub fn find_symbol_by_name(&self, name: &str) -> Option<SymbolRef> {
        let (_, entry) = self.binary_search_by_name(name)?;
        Some(SymbolRef {
            symbol_table: self,
            entry,
            kernel_base: get_kernel_base(),
        })
    }

    pub fn find_symbols_by_name(&self, name: &str) -> SymbolsByNameIterator {
        let (found_index, _) = match self.binary_search_by_name(name) {
            Some(result) => result,
            None => return SymbolsByNameIterator::empty(self),
        };

        let mut start = found_index;
        let mut end = found_index;

        while start > 0 {
            let entry = self.get_symbol_entry_by_name_index(start - 1);
            let symbol_name = entry.name(self);
            if symbol_name == name {
                start -= 1;
            } else {
                break;
            }
        }

        while end + 1 < self.symbol_count {
            let entry = self.get_symbol_entry_by_name_index(end + 1);
            let symbol_name = entry.name(self);
            if symbol_name == name {
                end += 1;
            } else {
                break;
            }
        }

        SymbolsByNameIterator {
            symbol_table: self,
            current_index: start,
            end_index: end + 1,
            kernel_base: get_kernel_base(),
        }
    }

    pub fn find_symbols_matching_glob(&self, pattern: &str) -> SymbolsMatchingIterator {
        if self.is_empty() {
            return SymbolsMatchingIterator::empty(self);
        }

        let pattern_cstr = CString::new(pattern).unwrap();
        let pspec = unsafe { g_pattern_spec_new(pattern_cstr.as_ptr()) };

        SymbolsMatchingIterator {
            symbol_table: self,
            current_index: 0,
            end_index: self.symbol_count,
            pspec,
            kernel_base: get_kernel_base(),
        }
    }

    pub fn find_symbol_by_address(&self, address: u64) -> Option<SymbolRef> {
        let target_offset = address - get_kernel_base();
        let (_, entry) = self.binary_search_by_address(target_offset)?;
        Some(SymbolRef {
            symbol_table: self,
            entry,
            kernel_base: get_kernel_base(),
        })
    }

    pub fn find_symbol_name_ptr_by_address(&self, address: u64) -> *const c_char {
        let target_offset = address - get_kernel_base();
        if let Some((_, entry)) = self.binary_search_by_address(target_offset) {
            entry.name_ptr(self)
        } else {
            ptr::null()
        }
    }

    pub fn find_closest_symbol_by_address(&self, address: u64) -> Option<SymbolRef> {
        let kernel_base = get_kernel_base();
        let target_offset = address - kernel_base;
        let (_, entry) = self.binary_search_closest_by_address(target_offset)?;
        Some(SymbolRef {
            symbol_table: self,
            entry,
            kernel_base,
        })
    }

    pub fn iter_symbols_in_range(&self, start_address: u64, end_address: u64) -> SymbolsInRangeIterator {
        if self.is_empty() {
            return SymbolsInRangeIterator {
                symbol_table: self,
                current_index: 0,
                end_index: 0,
                end_offset: 0,
                kernel_base: get_kernel_base(),
            };
        }

        let kernel_base = get_kernel_base();
        let start_offset = start_address - kernel_base;
        let end_offset = end_address - kernel_base;

        let mut left = 0;
        let mut right = self.symbol_count;

        while left < right {
            let mid = left + (right - left) / 2;
            let entry = self.get_symbol_entry_by_address_index(mid);

            if { entry.address_offset } < start_offset {
                left = mid + 1;
            } else {
                right = mid;
            }
        }

        while left > 0 {
            let prev_entry = self.get_symbol_entry_by_address_index(left - 1);
            if { prev_entry.address_offset } >= start_offset {
                left -= 1;
            } else {
                break;
            }
        }

        SymbolsInRangeIterator {
            symbol_table: self,
            current_index: left,
            end_index: self.symbol_count,
            end_offset,
            kernel_base,
        }
    }

    fn get_symbol_entry_by_name_index(&self, index: usize) -> &SymbolEntry {
        let offset = self.name_index_start() + index * 4;
        let symbol_offset = unsafe { *(self.data.as_ptr().add(offset) as *const u32) as usize };
        self.symbol_entry_at(symbol_offset)
    }

    fn get_symbol_entry_by_address_index(&self, index: usize) -> &SymbolEntry {
        let offset = self.address_index_start() + index * 4;
        let symbol_offset = unsafe { *(self.data.as_ptr().add(offset) as *const u32) as usize };
        self.symbol_entry_at(symbol_offset)
    }

    fn symbol_entry_at(&self, offset: usize) -> &SymbolEntry {
        unsafe { &*(self.data.as_ptr().add(offset) as *const SymbolEntry) }
    }

    fn name_index_start(&self) -> usize {
        4
    }

    fn address_index_start(&self) -> usize {
        4 + self.symbol_count * 4
    }

    fn binary_search_by_name(&self, name: &str) -> Option<(usize, &SymbolEntry)> {
        self.binary_search(
            |table, mid| table.get_symbol_entry_by_name_index(mid),
            |entry| entry.name(self).cmp(name),
            false,
        )
    }

    fn binary_search_by_address(&self, target_offset: u64) -> Option<(usize, &SymbolEntry)> {
        self.binary_search(
            |table, mid| table.get_symbol_entry_by_address_index(mid),
            |entry| { entry.address_offset }.cmp(&target_offset),
            false,
        )
    }

    fn binary_search_closest_by_address(
        &self,
        target_offset: u64,
    ) -> Option<(usize, &SymbolEntry)> {
        self.binary_search(
            |table, mid| table.get_symbol_entry_by_address_index(mid),
            |entry| { entry.address_offset }.cmp(&target_offset),
            true,
        )
    }

    fn binary_search<F, C>(
        &self,
        get_entry_fn: F,
        compare_fn: C,
        find_closest: bool,
    ) -> Option<(usize, &SymbolEntry)>
    where
        F: Fn(&Self, usize) -> &SymbolEntry,
        C: Fn(&SymbolEntry) -> Ordering,
    {
        if self.is_empty() {
            return None;
        }

        let mut left = 0;
        let mut right = self.symbol_count;
        let mut best_match: Option<(usize, &SymbolEntry)> = None;

        while left < right {
            let mid = left + (right - left) / 2;
            let entry = get_entry_fn(self, mid);

            match compare_fn(entry) {
                Ordering::Equal => return Some((mid, entry)),
                Ordering::Less => {
                    if find_closest {
                        best_match = Some((mid, entry));
                    }
                    left = mid + 1;
                }
                Ordering::Greater => right = mid,
            }
        }

        best_match
    }
}

// The host writes this layout, thus the compiler must add no padding.
#[repr(C, packed)]
struct SymbolEntry {
    address_offset: u64,
    symbol_type: u8,
    section: u8,
    description: u16,
}

impl SymbolEntry {
    fn name<'a>(&self, symbol_table: &'a SymbolTable) -> &'a str {
        unsafe {
            CStr::from_ptr(self.name_ptr(symbol_table))
                .to_str()
                .unwrap()
        }
    }

    fn name_ptr(&self, symbol_table: &SymbolTable) -> *const c_char {
        let entry_ptr = self as *const SymbolEntry as *const u8;
        let data_start = symbol_table.data.as_ptr();
        let entry_offset = unsafe { entry_ptr.offset_from(data_start) as usize };
        let name_start = entry_offset + size_of::<SymbolEntry>();

        unsafe { symbol_table.data.as_ptr().add(name_start) as *const c_char }
    }
}

pub struct SymbolRef<'a> {
    symbol_table: &'a SymbolTable,
    entry: &'a SymbolEntry,
    kernel_base: u64,
}

impl<'a> SymbolRef<'a> {
    pub fn name(&self) -> &'a str {
        self.entry.name(self.symbol_table)
    }

    pub fn name_ptr(&self) -> *const c_char {
        self.entry.name_ptr(self.symbol_table)
    }

    pub fn address(&self) -> u64 {
        self.kernel_base + { self.entry.address_offset }
    }

    pub fn symbol_type(&self) -> u8 {
        self.entry.symbol_type
    }

    pub fn section(&self) -> u8 {
        self.entry.section
    }

    pub fn description(&self) -> u16 {
        self.entry.description
    }
}

pub struct SymbolsByNameIterator<'a> {
    symbol_table: &'a SymbolTable,
    current_index: usize,
    end_index: usize,
    kernel_base: u64,
}

impl<'a> SymbolsByNameIterator<'a> {
    fn empty(symbol_table: &'a SymbolTable) -> Self {
        Self {
            symbol_table,
            current_index: 0,
            end_index: 0,
            kernel_base: get_kernel_base(),
        }
    }
}

impl<'a> Iterator for SymbolsByNameIterator<'a> {
    type Item = SymbolRef<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        if self.current_index == self.end_index {
            return None;
        }

        let entry = self.symbol_table.get_symbol_entry_by_name_index(self.current_index);
        self.current_index += 1;

        Some(SymbolRef {
            symbol_table: self.symbol_table,
            entry,
            kernel_base: self.kernel_base,
        })
    }
}

pub struct SymbolsMatchingIterator<'a> {
    symbol_table: &'a SymbolTable,
    current_index: usize,
    end_index: usize,
    pspec: *mut GPatternSpec,
    kernel_base: u64,
}

impl<'a> SymbolsMatchingIterator<'a> {
    fn empty(symbol_table: &'a SymbolTable) -> Self {
        Self {
            symbol_table,
            current_index: 0,
            end_index: 0,
            pspec: ptr::null_mut(),
            kernel_base: get_kernel_base(),
        }
    }
}

impl<'a> Iterator for SymbolsMatchingIterator<'a> {
    type Item = SymbolRef<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        while self.current_index != self.end_index {
            let entry = self.symbol_table.get_symbol_entry_by_name_index(self.current_index);
            self.current_index += 1;

            if unsafe { g_pattern_spec_match_string(self.pspec, entry.name_ptr(self.symbol_table)) } != 0 {
                return Some(SymbolRef {
                    symbol_table: self.symbol_table,
                    entry,
                    kernel_base: self.kernel_base,
                });
            }
        }

        None
    }
}

impl<'a> Drop for SymbolsMatchingIterator<'a> {
    fn drop(&mut self) {
        if !self.pspec.is_null() {
            unsafe { g_pattern_spec_free(self.pspec) };
        }
    }
}

pub struct SymbolsInRangeIterator<'a> {
    symbol_table: &'a SymbolTable,
    current_index: usize,
    end_index: usize,
    end_offset: u64,
    kernel_base: u64,
}

impl<'a> Iterator for SymbolsInRangeIterator<'a> {
    type Item = SymbolRef<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        while self.current_index != self.end_index {
            let entry = self.symbol_table.get_symbol_entry_by_address_index(self.current_index);
            self.current_index += 1;

            if { entry.address_offset } >= self.end_offset {
                break;
            }

            return Some(SymbolRef {
                symbol_table: self.symbol_table,
                entry,
                kernel_base: self.kernel_base,
            });
        }

        None
    }
}
