
use alloc::string::{String, ToString};
use alloc::vec::Vec;

use crate::bindings::{
    GumDarwinExportDetails, GumDarwinModule, GumMemoryRange, GumModuleRegistry, GumAddress,
    gboolean, g_object_unref, gpointer, gsize,
};
use crate::gum;

pub fn register_what_the_copy_lives_among(registry: *mut GumModuleRegistry) {
    for image in what_is_loaded() {
        let range = GumMemoryRange { base_address: image.base, size: image.size as gsize };
        let module = gum::gum_native_module_new(&image.path, "", &range);
        unsafe {
            crate::bindings::gum_barebone_register_module(registry, module);
            g_object_unref(module as gpointer);
        }
    }
}

pub fn enumerate_exports_in_range(from: u64, to: u64,
    found: &mut crate::gum::FoundExportCallback<'_>)
{
    let _ = to;
    let Some(image) = what_is_loaded().into_iter().find(|image| image.base == from) else {
        return;
    };

    let Some(module) = read_the_image(image.base, &image.path) else {
        return;
    };

    let mut asking = Asking { found, base: image.base };
    unsafe {
        crate::bindings::gum_darwin_module_enumerate_exports(module,
            Some(crate::signed_to_be_called_back(note_an_export, 0)),
            &mut asking as *mut Asking<'_, '_> as gpointer);
        g_object_unref(module as gpointer);
    }
}

struct Asking<'a, 'b> {
    found: &'a mut crate::gum::FoundExportCallback<'b>,
    base: u64,
}

unsafe extern "C" fn note_an_export(details: *const GumDarwinExportDetails, asking: gpointer)
    -> gboolean
{
    let asking = unsafe { &mut *(asking as *mut Asking<'_, '_>) };
    let details = unsafe { &*details };

    let offset = unsafe { details.__bindgen_anon_1.__bindgen_anon_1.offset };
    let named = unsafe { details.name.add(1) };
    (asking.found)(named, asking.base + offset);

    1
}

pub fn export_named(wanted: &str) -> u64 {
    let Some(ask_the_loader) = crate::xnu_libsystem::function_named(b"/libdyld.dylib", b"_dlsym")
    else {
        return 0;
    };

    type AskTheLoader = unsafe extern "C" fn(*const core::ffi::c_void, *const u8) -> u64;
    let ask_the_loader: AskTheLoader = unsafe { core::mem::transmute(ask_the_loader) };

    let Ok(asked) = alloc::ffi::CString::new(wanted) else {
        return 0;
    };

    let signed = unsafe { ask_the_loader(WHEREVER_IT_IS, asked.as_ptr() as *const u8) };

    unsafe { crate::pac::ptrauth_strip_data(signed as *const u8) as u64 }
}

const WHEREVER_IT_IS: *const core::ffi::c_void = -2isize as *const core::ffi::c_void;

struct Image {
    base: u64,
    size: u64,
    path: String,
}

fn what_is_loaded() -> Vec<Image> {
    let mut found = Vec::new();

    if !crate::xnu_user::on_a_thread_the_system_knows() {
        return found;
    }

    let Some(list) = where_the_loader_keeps_its_list() else {
        return found;
    };

    let images = read_word(list + HOW_MANY_IMAGES) as usize;
    let at = read_long(list + WHERE_THE_IMAGES_ARE);
    for step in 0..images.min(MOST_IMAGES) {
        let each = at + (step * AN_IMAGE) as u64;
        let base = read_long(each);
        let path = read_long(each + WHAT_IT_CAME_FROM);
        if base == 0 || path == 0 {
            continue;
        }

        let path = text_at(path);
        let size = how_much_of_it_there_is(base, &path);
        if size == 0 {
            continue;
        }
        found.push(Image { base, size, path });
    }

    let loader = read_long(list + WHERE_THE_LOADER_IS);
    if loader != 0 {
        let size = how_much_of_it_there_is(loader, WHAT_THE_LOADER_IS_CALLED);
        if size != 0 {
            found.push(Image { base: loader, size,
                path: WHAT_THE_LOADER_IS_CALLED.to_string() });
        }
    }

    found
}

fn where_the_loader_keeps_its_list() -> Option<u64> {
    let asking = crate::xnu_libsystem::asking_about_threads()?;
    let about: unsafe extern "C" fn(u32, u32, *mut u32, *mut u32) -> i32 =
        unsafe { core::mem::transmute(crate::xnu_libsystem::function_named(
            b"/libsystem_kernel.dylib", b"_task_info")?) };

    let task = unsafe { (asking.this_task as *const u32).read_volatile() };
    let mut said = [0u32; ABOUT_THE_LOADER_WORDS];
    let mut room = said.len() as u32;
    let told = unsafe { about(task, ABOUT_THE_LOADER, said.as_mut_ptr(), &mut room) };
    if told != 0 {
        return None;
    }

    let list = (said[0] as u64) | ((said[1] as u64) << 32);

    (list != 0).then_some(list)
}

fn read_the_image(base: u64, name: &str) -> Option<*mut GumDarwinModule> {
    let named = alloc::ffi::CString::new(name).ok()?;
    let module = unsafe {
        crate::bindings::gum_darwin_module_new_from_memory(named.as_ptr(), NO_TASK_TO_ASK,
            base as GumAddress, 0, core::ptr::null_mut())
    };

    (!module.is_null()).then_some(module)
}

fn how_much_of_it_there_is(base: u64, name: &str) -> u64 {
    let Some(module) = read_the_image(base, name) else {
        return 0;
    };

    let mut size = 0;
    for step in 0.. {
        let segment = unsafe { crate::bindings::gum_darwin_module_get_nth_segment(module, step) };
        if segment.is_null() {
            break;
        }
        if name_is(unsafe { &raw const (*segment).name } as u64, b"__TEXT") {
            size = unsafe { (*segment).vm_size };
            break;
        }
    }
    unsafe { g_object_unref(module as gpointer) };

    size
}

fn name_is(at: u64, wanted: &[u8]) -> bool {
    for (step, byte) in wanted.iter().enumerate() {
        if unsafe { ((at as *const u8).add(step)).read_volatile() } != *byte {
            return false;
        }
    }

    let past = unsafe { ((at as *const u8).add(wanted.len())).read_volatile() };

    past == 0
}

fn text_at(at: u64) -> String {
    let mut said = Vec::new();
    for step in 0..LONGEST_NAME {
        let byte = unsafe { ((at as *const u8).add(step)).read_volatile() };
        if byte == 0 {
            break;
        }
        said.push(byte);
    }

    core::str::from_utf8(&said).unwrap_or("?").to_string()
}

fn read_word(at: u64) -> u32 {
    unsafe { (at as *const u32).read_volatile() }
}

fn read_long(at: u64) -> u64 {
    unsafe { (at as *const u64).read_volatile() }
}

const NO_TASK_TO_ASK: u32 = 0;

const ABOUT_THE_LOADER: u32 = 17;
const ABOUT_THE_LOADER_WORDS: usize = 8;
const HOW_MANY_IMAGES: u64 = 4;
const WHERE_THE_LOADER_IS: u64 = 32;
const WHAT_THE_LOADER_IS_CALLED: &str = "/usr/lib/dyld";
const WHERE_THE_IMAGES_ARE: u64 = 8;
const AN_IMAGE: usize = 24;
const WHAT_IT_CAME_FROM: u64 = 8;
const MOST_IMAGES: usize = 4096;

const LONGEST_NAME: usize = 1024;

