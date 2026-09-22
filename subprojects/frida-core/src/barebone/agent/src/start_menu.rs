
use alloc::vec::Vec;

#[cfg(target_arch = "x86")]
macro_rules! windows_fn {
    ($($argument:ty),* $(,)? => $result:ty) => {
        unsafe extern "stdcall" fn($($argument),*) -> $result
    };
}

#[cfg(target_arch = "x86_64")]
macro_rules! windows_fn {
    ($($argument:ty),* $(,)? => $result:ty) => {
        unsafe extern "win64" fn($($argument),*) -> $result
    };
}

#[cfg(target_arch = "aarch64")]
macro_rules! windows_fn {
    ($($argument:ty),* $(,)? => $result:ty) => {
        unsafe extern "C" fn($($argument),*) -> $result
    };
}

pub struct Api {
    pub find_first: usize,
    pub find_next: usize,
    pub find_close: usize,
    pub create_file: usize,
    pub read_file: usize,
    pub set_file_pointer: usize,
    pub close_handle: usize,
}

pub fn enumerate(api: &Api, menu: &[u8], found: &mut dyn FnMut(&str, &str, &str, &str)) {
    walk_for_shortcuts(api, menu, 0, found);
}

fn identity_of(api: &Api, target: &[u8]) -> (Vec<u8>, Vec<u8>) {
    let mut asciiz = Vec::new();
    asciiz.extend_from_slice(target);
    asciiz.push(0);

    let Some(image) = Image::open(api, &asciiz) else {
        return (Vec::new(), Vec::new());
    };

    let mut identity = [0u8; 128];
    let mut description = [0u8; 128];
    let named = crate::icons::identify(&image, &mut identity);
    let described = crate::icons::describe(&image, &mut description);

    (identity[..named].to_vec(), description[..described].to_vec())
}

fn walk_for_shortcuts(api: &Api, directory: &[u8], depth: u32,
        found: &mut dyn FnMut(&str, &str, &str, &str)) {
    if depth == MAX_MENU_DEPTH {
        return;
    }

    let find_first: windows_fn!(*const u8, *mut u8 => u32) =
        unsafe { core::mem::transmute(api.find_first) };
    let find_next: windows_fn!(u32, *mut u8 => u32) =
        unsafe { core::mem::transmute(api.find_next) };
    let find_close: windows_fn!(u32 => u32) = unsafe { core::mem::transmute(api.find_close) };

    let mut pattern = Vec::new();
    pattern.extend_from_slice(directory);
    pattern.extend_from_slice(b"\\*.*\0");

    let mut entry = [0u8; FIND_DATA_SIZE];
    let search = unsafe { find_first(pattern.as_ptr(), entry.as_mut_ptr()) };
    if search == INVALID_HANDLE {
        return;
    }

    loop {
        let attributes = u32::from_le_bytes([entry[0], entry[1], entry[2], entry[3]]);
        let name = text_in(&entry[FIND_DATA_NAME..]);

        if name != b"." && name != b".." {
            let mut path = Vec::new();
            path.extend_from_slice(directory);
            path.push(b'\\');
            path.extend_from_slice(name);

            if attributes & FILE_ATTRIBUTE_DIRECTORY != 0 {
                walk_for_shortcuts(api, &path, depth + 1, found);
            } else if ends_with_ignoring_case(name, b".lnk") {
                if let Some(target) = target_of_shortcut(api, &path)
                        .filter(|target| ends_with_ignoring_case(target, b".exe")) {
                    let shown = &name[..name.len() - 4];
                    let (identity, description) = identity_of(api, &target);
                    found(text_as_str(&identity), text_as_str(&target), text_as_str(shown),
                        text_as_str(&description));
                }
            }
        }

        if unsafe { find_next(search, entry.as_mut_ptr()) } == 0 {
            break;
        }
    }

    unsafe { find_close(search) };
}

fn target_of_shortcut(api: &Api, path: &[u8]) -> Option<Vec<u8>> {
    let mut asciiz = Vec::new();
    asciiz.extend_from_slice(path);
    asciiz.push(0);

    let blob = read_whole_file(api, &asciiz)?;
    if blob.len() < SHELL_LINK_HEADER || blob[0] != SHELL_LINK_HEADER as u8 {
        return None;
    }

    let flags = u32::from_le_bytes([blob[0x14], blob[0x15], blob[0x16], blob[0x17]]);
    let mut at = SHELL_LINK_HEADER;

    if flags & HAS_TARGET_ID_LIST != 0 {
        if at + 2 > blob.len() {
            return None;
        }
        at += 2 + u16::from_le_bytes([blob[at], blob[at + 1]]) as usize;
    }

    if flags & HAS_LINK_INFO == 0 || at + 0x1c > blob.len() {
        return None;
    }

    let word = |offset: usize| {
        u32::from_le_bytes([blob[offset], blob[offset + 1], blob[offset + 2], blob[offset + 3]])
    };
    if word(at + 8) & VOLUME_ID_AND_LOCAL_BASE_PATH == 0 {
        return None;
    }

    let base = at + word(at + 0x10) as usize;
    let suffix = at + word(at + 0x18) as usize;
    if base >= blob.len() || suffix >= blob.len() {
        return None;
    }

    let mut target = Vec::new();
    target.extend_from_slice(text_in(&blob[base..]));
    target.extend_from_slice(text_in(&blob[suffix..]));

    Some(target)
}

fn read_whole_file(api: &Api, path: &[u8]) -> Option<Vec<u8>> {
    let create_file: windows_fn!(*const u8, u32, u32, u32, u32, u32, u32 => u32) =
        unsafe { core::mem::transmute(api.create_file) };
    let read_file: windows_fn!(u32, *mut u8, u32, *mut u32, u32 => u32) =
        unsafe { core::mem::transmute(api.read_file) };
    let close_handle: windows_fn!(u32 => u32) = unsafe { core::mem::transmute(api.close_handle) };

    let file = unsafe {
        create_file(path.as_ptr(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0)
    };
    if file == INVALID_HANDLE {
        return None;
    }

    let mut blob = alloc::vec![0u8; MAX_SHORTCUT];
    let mut taken = 0u32;
    let ok = unsafe {
        read_file(file, blob.as_mut_ptr(), blob.len() as u32, &raw mut taken, 0)
    };
    unsafe { close_handle(file) };

    if ok == 0 {
        return None;
    }
    blob.truncate(taken as usize);

    Some(blob)
}

fn text_in(bytes: &[u8]) -> &[u8] {
    let end = bytes.iter().position(|b| *b == 0).unwrap_or(bytes.len());

    &bytes[..end]
}

fn text_as_str(bytes: &[u8]) -> &str {
    core::str::from_utf8(bytes).unwrap_or("")
}

fn ends_with_ignoring_case(name: &[u8], suffix: &[u8]) -> bool {
    if name.len() < suffix.len() {
        return false;
    }

    name[name.len() - suffix.len()..]
        .iter()
        .zip(suffix)
        .all(|(a, b)| a.to_ascii_lowercase() == *b)
}

const MAX_MENU_DEPTH: u32 = 4;
const MAX_SHORTCUT: usize = 4096;
const FIND_DATA_SIZE: usize = 0x140;
const FIND_DATA_NAME: usize = 0x2c;
const FILE_ATTRIBUTE_DIRECTORY: u32 = 0x10;
const INVALID_HANDLE: u32 = 0xffff_ffff;
const GENERIC_READ: u32 = 0x8000_0000;
const FILE_SHARE_READ: u32 = 0x1;
const OPEN_EXISTING: u32 = 3;
const SHELL_LINK_HEADER: usize = 0x4c;
const HAS_TARGET_ID_LIST: u32 = 0x1;
const HAS_LINK_INFO: u32 = 0x2;
const VOLUME_ID_AND_LOCAL_BASE_PATH: u32 = 0x1;

struct Image<'a> {
    api: &'a Api,
    handle: u32,
}

impl crate::icons::Image for Image<'_> {
    fn read_at(&self, position: u32, buffer: &mut [u8]) -> u32 {
        let set_pointer: windows_fn!(u32, u32, u32, u32 => u32) =
            unsafe { core::mem::transmute(self.api.set_file_pointer) };
        let read_file: windows_fn!(u32, *mut u8, u32, *mut u32, u32 => u32) =
            unsafe { core::mem::transmute(self.api.read_file) };

        unsafe { set_pointer(self.handle, position, 0, 0) };

        let mut taken = 0u32;
        if unsafe { read_file(self.handle, buffer.as_mut_ptr(), buffer.len() as u32,
                &raw mut taken, 0) } == 0 {
            return 0;
        }

        taken
    }
}

impl<'a> Image<'a> {
    fn open(api: &'a Api, path: &[u8]) -> Option<Image<'a>> {
        let create_file: windows_fn!(*const u8, u32, u32, u32, u32, u32, u32 => u32) =
            unsafe { core::mem::transmute(api.create_file) };

        let handle = unsafe {
            create_file(path.as_ptr(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0)
        };
        if handle == INVALID_HANDLE {
            return None;
        }

        Some(Image { api, handle })
    }
}

impl Drop for Image<'_> {
    fn drop(&mut self) {
        let close_handle: windows_fn!(u32 => u32) =
            unsafe { core::mem::transmute(self.api.close_handle) };
        unsafe { close_handle(self.handle) };
    }
}

