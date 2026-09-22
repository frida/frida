
pub trait Image {
    fn read_at(&self, position: u32, buffer: &mut [u8]) -> u32;
}

pub fn enumerate(image: &dyn Image, found: &mut dyn FnMut(&[u8])) {
    let Some(resources) = resource_directory(image) else {
        return;
    };

    let Some(group) = resources.first_leaf(RT_GROUP_ICON) else {
        return;
    };

    // A group contains each size one time for each color depth. Keep only the best of each size.
    let count = read_u16(image, group.offset + GROUP_COUNT_OFFSET) as u32;
    for index in 0..count {
        if !is_best_of_its_size(image, group.offset, count, index) {
            continue;
        }

        let entry = group.offset + GROUP_ENTRIES_OFFSET + index * GROUP_ENTRY_SIZE;
        let id = read_u16(image, entry + GROUP_ENTRY_ID_OFFSET);
        if let Some(icon) = resources.leaf(RT_ICON, id) {
            if let Some(bytes) = read_blob(image, icon.offset, icon.size) {
                found(bytes);
            }
        }
    }
}

pub fn describe(image: &dyn Image, into: &mut [u8]) -> usize {
    if read_u16(image, read_u32(image, DOS_HEADERS_OFFSET)) == NE_SIGNATURE {
        return describe_ne(image, into);
    }

    string_in_version(image, FILE_DESCRIPTION, into)
}

pub fn identify(image: &dyn Image, into: &mut [u8]) -> usize {
    if read_u16(image, read_u32(image, DOS_HEADERS_OFFSET)) == NE_SIGNATURE {
        let mut description = [0u8; 128];
        let taken = describe_ne(image, &mut description);
        let described = &description[..taken];
        let Some(space) = described.iter().position(|b| *b == b' ') else {
            return 0;
        };

        let program = &described[space + 1..];
        if program.iter().filter(|b| b.is_ascii_alphanumeric()).count() > MAX_PROGRAM_WORD {
            return 0;
        }

        return write_identity(&described[..space], program, into);
    }

    let mut company = [0u8; 64];
    let mut taken_company = string_in_version(image, COMPANY_NAME, &mut company);
    if taken_company == 0 {
        taken_company = string_in_version(image, PRODUCT_NAME, &mut company);
    }

    let mut internal = [0u8; 64];
    let mut taken_internal = string_in_version(image, INTERNAL_NAME, &mut internal);
    if taken_internal == 0 {
        taken_internal = string_in_version(image, ORIGINAL_FILENAME, &mut internal);
    }

    if taken_company == 0 || taken_internal == 0 {
        return 0;
    }

    write_identity(&company[..taken_company], without_extension(&internal[..taken_internal]), into)
}

fn without_extension(name: &[u8]) -> &[u8] {
    match name.iter().rposition(|b| *b == b'.') {
        Some(dot) => &name[..dot],
        None => name,
    }
}

fn write_identity(company: &[u8], program: &[u8], into: &mut [u8]) -> usize {
    let maker = match company.iter().position(|b| *b == b' ') {
        Some(space) => &company[..space],
        None => company,
    };

    let mut written = 0;
    let mut put = |byte: u8, into: &mut [u8], written: &mut usize| {
        if *written != into.len() {
            into[*written] = byte;
            *written += 1;
        }
    };
    let mut put_word = |word: &[u8], into: &mut [u8], written: &mut usize| {
        for byte in word {
            if byte.is_ascii_alphanumeric() {
                put(byte.to_ascii_lowercase(), into, written);
            }
        }
    };

    put_word(b"com", into, &mut written);
    put(b'.', into, &mut written);
    put_word(maker, into, &mut written);
    put(b'.', into, &mut written);
    put_word(program, into, &mut written);

    written
}

fn string_in_version(image: &dyn Image, key: &[u8], into: &mut [u8]) -> usize {
    let Some(resources) = resource_directory(image) else {
        return 0;
    };
    let Some(version) = resources.first_leaf(RT_VERSION) else {
        return 0;
    };
    let Some(blob) = read_blob(image, version.offset, version.size) else {
        return 0;
    };
    let Some(at) = find(blob, key) else {
        return 0;
    };

    // Each string in the block says how long its value is, in the six bytes before its key. A
    // value of nothing leaves the next key where the value would be.
    if at < 6 {
        return 0;
    }
    let value_length = u16::from_le_bytes([blob[at - 4], blob[at - 3]]) as usize;
    if value_length == 0 {
        return 0;
    }

    let mut at = at + key.len();
    while at % 4 != 0 {
        at += 1;
    }

    let mut written = 0;
    while at + 1 < blob.len() && written < into.len() && written < value_length {
        let unit = blob[at] as u16 | ((blob[at + 1] as u16) << 8);
        if unit == 0 {
            break;
        }
        into[written] = if unit < 0x80 { unit as u8 } else { b'?' };
        written += 1;
        at += 2;
    }

    written
}

fn describe_ne(image: &dyn Image, into: &mut [u8]) -> usize {
    let headers = read_u32(image, DOS_HEADERS_OFFSET);
    let table = read_u32(image, headers + NE_NON_RESIDENT_NAMES_OFFSET);
    if table == 0 {
        return 0;
    }

    let mut length = [0u8; 1];
    if image.read_at(table, &mut length) != 1 {
        return 0;
    }

    let wanted = core::cmp::min(length[0] as usize, into.len());
    image.read_at(table + 1, &mut into[..wanted]) as usize
}

fn find(haystack: &[u8], needle: &[u8]) -> Option<usize> {
    haystack.windows(needle.len()).position(|window| window == needle)
}

fn is_best_of_its_size(image: &dyn Image, group: u32, count: u32, index: u32) -> bool {
    let mine = entry_shape(image, group, index);

    (0..count).filter(|other| *other != index).all(|other| {
        let theirs = entry_shape(image, group, other);
        theirs.0 != mine.0 || theirs.1 != mine.1 || theirs.2 < mine.2
            || (theirs.2 == mine.2 && other > index)
    })
}

fn entry_shape(image: &dyn Image, group: u32, index: u32) -> (u8, u8, u16) {
    let entry = group + GROUP_ENTRIES_OFFSET + index * GROUP_ENTRY_SIZE;

    (
        read_u8(image, entry + GROUP_ENTRY_WIDTH_OFFSET),
        read_u8(image, entry + GROUP_ENTRY_HEIGHT_OFFSET),
        read_u16(image, entry + GROUP_ENTRY_DEPTH_OFFSET),
    )
}

// A resource has the address that it gets after the loader maps it. The section gives the
// distance from that address to the position in the file.
fn resource_directory(image: &dyn Image) -> Option<ResourceDirectory<'_>> {
    let headers = read_u32(image, DOS_HEADERS_OFFSET);
    if read_u32(image, headers) != PE_SIGNATURE {
        return None;
    }

    let optional = headers + OPTIONAL_HEADER_OFFSET;
    let directory_rva = read_u32(image, optional + resource_directory_offset(image, optional));
    if directory_rva == 0 {
        return None;
    }

    let sections = optional + read_u16(image, headers + OPTIONAL_HEADER_SIZE_OFFSET) as u32;
    let section_count = read_u16(image, headers + SECTION_COUNT_OFFSET) as u32;
    for index in 0..section_count {
        let section = sections + index * SECTION_SIZE;
        let virtual_address = read_u32(image, section + SECTION_RVA_OFFSET);
        let virtual_size = read_u32(image, section + SECTION_VIRTUAL_SIZE_OFFSET);
        if directory_rva >= virtual_address && directory_rva < virtual_address + virtual_size {
            let raw_data = read_u32(image, section + SECTION_RAW_DATA_OFFSET);
            return Some(ResourceDirectory {
                image,
                rva: directory_rva,
                offset: directory_rva - virtual_address + raw_data,
            });
        }
    }

    None
}

// The magic value gives the form of the optional header, and the two forms have different
// widths before the data directories.
fn resource_directory_offset(image: &dyn Image, optional: u32) -> u32 {
    if read_u16(image, optional) == PE32_MAGIC {
        return PE32_RESOURCE_DIRECTORY_OFFSET;
    }

    PE32PLUS_RESOURCE_DIRECTORY_OFFSET
}

struct ResourceDirectory<'i> {
    image: &'i dyn Image,
    rva: u32,
    offset: u32,
}

struct Resource {
    offset: u32,
    size: u32,
}

impl ResourceDirectory<'_> {
    fn first_leaf(&self, kind: u16) -> Option<Resource> {
        let by_name = self.subdirectory(self.offset, kind)?;
        let by_language = self.first_subdirectory(by_name)?;

        self.leaf_at(self.first_subdirectory(by_language)?)
    }

    fn leaf(&self, kind: u16, name: u16) -> Option<Resource> {
        let by_name = self.subdirectory(self.offset, kind)?;
        let by_language = self.subdirectory(by_name, name)?;

        self.leaf_at(self.first_subdirectory(by_language)?)
    }

    fn subdirectory(&self, directory: u32, id: u16) -> Option<u32> {
        for entry in self.entries(directory) {
            if read_u32(self.image, entry) == id as u32 {
                return Some(self.child(entry));
            }
        }

        None
    }

    fn first_subdirectory(&self, directory: u32) -> Option<u32> {
        self.entries(directory).next().map(|e| self.child(e))
    }

    fn entries(&self, directory: u32) -> impl Iterator<Item = u32> + '_ {
        let named = read_u16(self.image, directory + NAMED_ENTRY_COUNT_OFFSET) as u32;
        let identified = read_u16(self.image, directory + ID_ENTRY_COUNT_OFFSET) as u32;
        let first = directory + DIRECTORY_HEADER_SIZE + named * DIRECTORY_ENTRY_SIZE;

        (0..identified).map(move |index| first + index * DIRECTORY_ENTRY_SIZE)
    }

    fn child(&self, entry: u32) -> u32 {
        let value = read_u32(self.image, entry + DIRECTORY_ENTRY_CHILD_OFFSET);
        self.offset + (value & !SUBDIRECTORY_FLAG)
    }

    fn leaf_at(&self, data_entry: u32) -> Option<Resource> {
        let data_rva = read_u32(self.image, data_entry);

        Some(Resource {
            offset: self.offset + data_rva - self.rva,
            size: read_u32(self.image, data_entry + DATA_ENTRY_SIZE_OFFSET),
        })
    }
}

fn read_u32(image: &dyn Image, position: u32) -> u32 {
    let mut bytes = [0u8; 4];
    image.read_at(position, &mut bytes);
    u32::from_le_bytes(bytes)
}

fn read_u8(image: &dyn Image, position: u32) -> u8 {
    let mut bytes = [0u8; 1];
    image.read_at(position, &mut bytes);
    bytes[0]
}

fn read_u16(image: &dyn Image, position: u32) -> u16 {
    let mut bytes = [0u8; 2];
    image.read_at(position, &mut bytes);
    u16::from_le_bytes(bytes)
}

fn read_blob(image: &dyn Image, position: u32, size: u32) -> Option<&'static [u8]> {
    let blob = unsafe { &mut *core::ptr::addr_of_mut!(ICON_BUFFER) };
    if size as usize > blob.len() {
        return None;
    }

    let read = image.read_at(position, &mut blob[..size as usize]);
    if read != size {
        return None;
    }

    Some(&blob[..size as usize])
}

static mut ICON_BUFFER: [u8; MAX_ICON_SIZE] = [0; MAX_ICON_SIZE];

const MAX_ICON_SIZE: usize = 16 * 1024;

const DOS_HEADERS_OFFSET: u32 = 0x3c;
const PE_SIGNATURE: u32 = 0x00004550;
const SECTION_COUNT_OFFSET: u32 = 0x06;
const OPTIONAL_HEADER_SIZE_OFFSET: u32 = 0x14;
const OPTIONAL_HEADER_OFFSET: u32 = 0x18;
const PE32_MAGIC: u16 = 0x010b;
const PE32_RESOURCE_DIRECTORY_OFFSET: u32 = 0x70;
const PE32PLUS_RESOURCE_DIRECTORY_OFFSET: u32 = 0x80;
const SECTION_SIZE: u32 = 0x28;
const SECTION_VIRTUAL_SIZE_OFFSET: u32 = 0x08;
const SECTION_RVA_OFFSET: u32 = 0x0c;
const SECTION_RAW_DATA_OFFSET: u32 = 0x14;

const RT_ICON: u16 = 3;
const RT_GROUP_ICON: u16 = 14;
const RT_VERSION: u16 = 16;
const NE_SIGNATURE: u16 = 0x454e;
const MAX_PROGRAM_WORD: usize = 24;
const NE_NON_RESIDENT_NAMES_OFFSET: u32 = 0x2c;
const COMPANY_NAME: &[u8] = b"C\0o\0m\0p\0a\0n\0y\0N\0a\0m\0e\0\0\0";
const INTERNAL_NAME: &[u8] = b"I\0n\0t\0e\0r\0n\0a\0l\0N\0a\0m\0e\0\0\0";
const PRODUCT_NAME: &[u8] = b"P\0r\0o\0d\0u\0c\0t\0N\0a\0m\0e\0\0\0";
const ORIGINAL_FILENAME: &[u8] =
    b"O\0r\0i\0g\0i\0n\0a\0l\0F\0i\0l\0e\0n\0a\0m\0e\0\0\0";

const FILE_DESCRIPTION: &[u8] = b"F\0i\0l\0e\0D\0e\0s\0c\0r\0i\0p\0t\0i\0o\0n\0\0\0";
const NAMED_ENTRY_COUNT_OFFSET: u32 = 0x0c;
const ID_ENTRY_COUNT_OFFSET: u32 = 0x0e;
const DIRECTORY_HEADER_SIZE: u32 = 0x10;
const DIRECTORY_ENTRY_SIZE: u32 = 0x08;
const DIRECTORY_ENTRY_CHILD_OFFSET: u32 = 0x04;
const SUBDIRECTORY_FLAG: u32 = 0x8000_0000;
const DATA_ENTRY_SIZE_OFFSET: u32 = 0x04;
const GROUP_COUNT_OFFSET: u32 = 0x04;
const GROUP_ENTRIES_OFFSET: u32 = 0x06;
const GROUP_ENTRY_SIZE: u32 = 0x0e;
const GROUP_ENTRY_WIDTH_OFFSET: u32 = 0x00;
const GROUP_ENTRY_HEIGHT_OFFSET: u32 = 0x01;
const GROUP_ENTRY_DEPTH_OFFSET: u32 = 0x06;
const GROUP_ENTRY_ID_OFFSET: u32 = 0x0c;
