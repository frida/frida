
use crate::kernel;

pub fn enumerate_ranges(found: &mut dyn FnMut(u64, usize, u32)) {
    for (address, size, protection) in kernel::noted_mappings() {
        found(*address, *size, *protection);
    }
}

pub fn protection_at(address: u64) -> u32 {
    kernel::noted_mappings()
        .iter()
        .find(|(base, size, _)| address >= *base && address < *base + *size as u64)
        .map(|(_, _, protection)| *protection)
        .unwrap_or(0)
}
