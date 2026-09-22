use alloc::string::String;
use alloc::vec::Vec;

use core::ffi::c_void;
use core::ptr;

use crate::bindings::{
    GError, GumElfModule, GumExportDetails, GumMemoryRange, GumModuleRegistry, gboolean,
    g_bytes_new, g_bytes_unref, g_clear_error, g_object_unref, gconstpointer, gpointer, gsize,
    gum_barebone_register_module, gum_barebone_unregister_module,
    gum_elf_module_enumerate_exports, gum_elf_module_enumerate_symbols,
    gum_elf_module_get_preferred_address, gum_elf_module_new_from_blob, gum_interceptor_begin_transaction,
    gum_interceptor_end_transaction, gum_interceptor_obtain, gum_interceptor_replace,
};
use crate::bindings::{GumElfSymbolBind_GUM_ELF_BIND_LOCAL, GumElfSymbolDetails};
use crate::gum::{self, FoundExportCallback, FoundSymbolCallback};
use alloc::ffi::CString;

use super::user::{EXECUTABLE, READABLE, WRITABLE, contents_of};

pub fn register_what_the_copy_lives_among(registry: *mut GumModuleRegistry) {
    unsafe { REGISTRY = registry };

    for image in mapped_images() {
        announce(&image);
        unsafe { known() }.push(image);
    }
}

// The loader calls the same empty function every time it has changed what is mapped -- that is
// what a debugger watches -- so the copy asks again from there.
pub fn watch_the_loader() {
    let Some(rendezvous) = where_the_loader_says_so() else {
        return;
    };

    unsafe {
        let interceptor = gum_interceptor_obtain();
        gum_interceptor_begin_transaction(interceptor);
        gum_interceptor_replace(interceptor, rendezvous as gpointer,
            the_loader_changed_something as gpointer, &raw mut LOADER_SAYS_SO,
            ptr::null_mut());
        gum_interceptor_end_transaction(interceptor);
    }
}

unsafe extern "C" fn the_loader_changed_something() {
    let said_so: unsafe extern "C" fn() = unsafe { core::mem::transmute(LOADER_SAYS_SO) };
    unsafe { said_so() };

    look_again();
}

fn look_again() {
    let registry = unsafe { REGISTRY };
    if registry.is_null() {
        return;
    }

    let mapped = mapped_images();
    let known = unsafe { known() };

    for image in &mapped {
        if !known.iter().any(|seen| seen.base == image.base) {
            announce(image);
        }
    }
    for seen in known.iter() {
        if !mapped.iter().any(|image| image.base == seen.base) {
            unsafe { gum_barebone_unregister_module(registry, seen.base) };
        }
    }

    *known = mapped;
}

fn announce(image: &Image) {
    let range = GumMemoryRange {
        base_address: image.base,
        size: image.size as gsize,
    };

    let module = gum::gum_native_module_new(&image.path, "", &range);
    unsafe {
        gum_barebone_register_module(unsafe { REGISTRY }, module);
        g_object_unref(module as gpointer);
    }
}

pub fn enumerate_exports_in_range(from: u64, to: u64, found: &mut FoundExportCallback<'_>) {
    let Some(image) = mapped_images().into_iter().find(|image| image.base == from) else {
        return;
    };
    let Some(module) = read_the_image(&image) else {
        return;
    };

    let mut asking = Asking {
        found,
        slide: slide_of(&image, module),
        from,
        to,
    };
    unsafe {
        gum_elf_module_enumerate_exports(
            module,
            Some(crate::signed_to_be_called_back(note_an_export, 0)),
            &mut asking as *mut Asking<'_, '_> as gpointer,
        );
        g_object_unref(module as gpointer);
    }
}

pub fn enumerate_symbols_in_range(from: u64, to: u64, found: &mut FoundSymbolCallback<'_>) {
    let Some(image) = mapped_images().into_iter().find(|image| image.base == from) else {
        return;
    };
    let Some(module) = read_the_image(&image) else {
        return;
    };

    let mut asking = AskingForSymbols {
        found,
        slide: slide_of(&image, module),
        from,
        to,
    };
    unsafe {
        gum_elf_module_enumerate_symbols(
            module,
            Some(crate::signed_to_be_called_back(note_a_symbol, 0)),
            &mut asking as *mut AskingForSymbols<'_, '_> as gpointer,
        );
        g_object_unref(module as gpointer);
    }
}

struct AskingForSymbols<'a, 'b> {
    found: &'a mut FoundSymbolCallback<'b>,
    slide: u64,
    from: u64,
    to: u64,
}

unsafe extern "C" fn note_a_symbol(
    details: *const GumElfSymbolDetails,
    asking: gpointer,
) -> gboolean {
    let asking = unsafe { &mut *(asking as *mut AskingForSymbols<'_, '_>) };
    let details = unsafe { &*details };

    if details.address == 0 {
        return 1;
    }

    let address = details.address.wrapping_add(asking.slide);
    if address < asking.from || address >= asking.to {
        return 1;
    }

    (asking.found)(
        details.name,
        address,
        details.size as u64,
        details.type_ as u8,
        details.bind != GumElfSymbolBind_GUM_ELF_BIND_LOCAL,
    ) as gboolean
}

fn slide_of(image: &Image, module: *mut GumElfModule) -> u64 {
    image
        .base
        .wrapping_sub(unsafe { gum_elf_module_get_preferred_address(module) })
}

fn read_the_image(image: &Image) -> Option<*mut GumElfModule> {
    let path = CString::new(image.path.as_str()).ok()?;
    let data = contents_of(&path);
    if data.is_empty() {
        return None;
    }

    unsafe {
        let blob = g_bytes_new(data.as_ptr() as gconstpointer, data.len() as gsize);

        let mut error: *mut GError = ptr::null_mut();
        let module = gum_elf_module_new_from_blob(blob, &mut error);
        g_bytes_unref(blob);

        if module.is_null() {
            g_clear_error(&mut error);
            return None;
        }

        Some(module)
    }
}

struct Asking<'a, 'b> {
    found: &'a mut FoundExportCallback<'b>,
    slide: u64,
    from: u64,
    to: u64,
}

unsafe extern "C" fn note_an_export(details: *const GumExportDetails, asking: gpointer) -> gboolean {
    let asking = unsafe { &mut *(asking as *mut Asking<'_, '_>) };
    let details = unsafe { &*details };

    let address = details.address.wrapping_add(asking.slide);
    if address < asking.from || address >= asking.to {
        return 1;
    }

    (asking.found)(details.name, address) as gboolean
}

fn where_the_loader_says_so() -> Option<u64> {
    let mut found = 0u64;
    for image in mapped_images() {
        let mut on_export = |name: *const crate::bindings::gchar, address: u64| {
            if unsafe { core::ffi::CStr::from_ptr(name) } == c"_dl_debug_state" {
                found = address;
            }

            found == 0
        };
        enumerate_exports_in_range(image.base, image.base + image.size, &mut on_export);
        if found != 0 {
            return Some(found);
        }
    }

    None
}

unsafe fn known() -> &'static mut Vec<Image> {
    unsafe { (&raw mut KNOWN).as_mut().unwrap() }
}

static mut KNOWN: Vec<Image> = Vec::new();
static mut REGISTRY: *mut GumModuleRegistry = ptr::null_mut();
static mut LOADER_SAYS_SO: *mut c_void = ptr::null_mut();

pub fn enumerate_ranges(found: &mut dyn FnMut(u64, u64, u32)) {
    let listed = contents_of(c"/proc/self/maps");

    for line in listed.split(|byte| *byte == b'\n') {
        let Some(mapping) = mapping_in(line) else {
            continue;
        };
        if super::user::range_is_ours(mapping.start) {
            continue;
        }

        found(mapping.start, mapping.end - mapping.start, mapping.protection);
    }
}

pub fn protection_at(address: u64) -> u32 {
    if super::user::range_is_ours(address) {
        return 0;
    }

    let listed = contents_of(c"/proc/self/maps");

    for line in listed.split(|byte| *byte == b'\n') {
        let Some(mapping) = mapping_in(line) else {
            continue;
        };

        if address >= mapping.start && address < mapping.end {
            return mapping.protection;
        }
    }

    0
}

fn mapped_images() -> Vec<Image> {
    let listed = contents_of(c"/proc/self/maps");

    let mut images: Vec<Image> = Vec::new();
    for line in listed.split(|byte| *byte == b'\n') {
        let Some(mapping) = mapping_in(line) else {
            continue;
        };
        if mapping.path.is_empty() || super::user::range_is_ours(mapping.start) {
            continue;
        }

        let holds_code = mapping.protection & EXECUTABLE as u32 != 0;
        match images.last_mut() {
            Some(image) if image.path.as_bytes() == mapping.path => {
                image.size = mapping.end - image.base;
                image.holds_code |= holds_code;
            }
            _ => images.push(Image {
                path: String::from_utf8_lossy(mapping.path).into_owned(),
                base: mapping.start,
                size: mapping.end - mapping.start,
                holds_code,
            }),
        }
    }

    images.retain(|image| image.holds_code);

    images
}

fn mapping_in(line: &[u8]) -> Option<Mapping<'_>> {
    let boundary = line.iter().position(|byte| *byte == b'-')?;
    let start = number_in(&line[..boundary])?;

    let rest = &line[boundary + 1..];
    let mut fields = rest.split(|byte| *byte == b' ');
    let end = number_in(fields.next()?)?;
    let how = fields.next()?;

    Some(Mapping {
        path: line[..].iter().position(|byte| *byte == b'/').map_or(&[], |at| &line[at..]),
        start,
        end,
        protection: protection_of(how),
    })
}

fn protection_of(how: &[u8]) -> u32 {
    let mut protection = 0;
    if how[0] == b'r' {
        protection |= READABLE as u32;
    }
    if how[1] == b'w' {
        protection |= WRITABLE as u32;
    }
    if how[2] == b'x' {
        protection |= EXECUTABLE as u32;
    }

    protection
}

fn number_in(digits: &[u8]) -> Option<u64> {
    let mut value = 0u64;
    for digit in digits {
        value = (value << 4) | (digit.to_ascii_lowercase() as char).to_digit(16)? as u64;
    }

    Some(value)
}

struct Image {
    path: String,
    base: u64,
    size: u64,
    holds_code: bool,
}

struct Mapping<'a> {
    path: &'a [u8],
    start: u64,
    end: u64,
    protection: u32,
}
