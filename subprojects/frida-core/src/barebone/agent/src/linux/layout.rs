use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use core::ffi::c_void;

pub fn field_offset(container: &str, field: &str) -> Option<usize> {
    let types = described_types()?;
    let described = types.named(container)?;

    types.offset_of(&described, field).map(|bits| bits / 8)
}

pub fn resolve_struct(container: &str) -> Option<usize> {
    let described = described_types()?.named(container)?;

    let resolved = unsafe { (&raw mut RESOLVED).as_mut().unwrap() };
    resolved.push(described);

    Some(resolved.len() - 1)
}

pub fn size_of_struct(handle: usize) -> usize {
    resolved_struct(handle).size as usize
}

pub fn field_offset_in(handle: usize, field: &str) -> Option<usize> {
    let types = described_types()?;

    types
        .offset_of(&resolved_struct(handle), field)
        .map(|bits| bits / 8)
}

pub fn enumerate_fields_in(handle: usize, on_field: &mut dyn FnMut(&str, &Field) -> bool) {
    let Some(types) = described_types() else {
        return;
    };

    types.visit_fields(&resolved_struct(handle), 0, on_field);
}

pub fn find_constant(name: &str) -> Option<i64> {
    described_types()?.constant_named(name)
}

pub fn enumerate_constants_in(container: &str, on_constant: &mut dyn FnMut(&str, i64)) -> bool {
    let Some(types) = described_types() else {
        return false;
    };
    let Some(described) = types.enum_named(container) else {
        return false;
    };

    types.visit_constants(&described, &mut |name, value| {
        on_constant(name, value);
        true
    });

    true
}

pub fn enumerate_parameters_in(
    name: &str,
    on_parameter: &mut dyn FnMut(&str, &str),
) -> Option<String> {
    let types = described_types()?;
    let described = types.function_named(name)?;
    let signature = types.with_id(described.size)?;

    types.visit_parameters(&signature, on_parameter);

    Some(types.render(signature.size, 0))
}

pub fn name_of_type(id: u32) -> String {
    described_types().map_or_else(String::new, |types| types.render(id, 0))
}

pub fn size_of_type(id: u32) -> usize {
    described_types().map_or(0, |types| types.span_of(id, 0))
}

pub struct Field {
    pub offset: usize,
    pub bit_offset: usize,
    pub bit_size: usize,
    pub id: u32,
}

fn resolved_struct(handle: usize) -> Described {
    unsafe { (&raw const RESOLVED).as_ref().unwrap()[handle].clone() }
}

pub fn types_are_described() -> bool {
    described_types().is_some()
}

pub fn struct_size(container: &str) -> Option<usize> {
    let types = described_types()?;

    Some(types.named(container)?.size as usize)
}

struct DescribedTypes {
    types: &'static [u8],
    names: &'static [u8],
}

impl DescribedTypes {
    fn named(&self, container: &str) -> Option<Described> {
        let index = self.index();

        if let Some(offset) = index.containers.get(container) {
            return self.at(*offset as usize);
        }

        while let Some(described) = self.take_in_next_type(index) {
            if self.names_container(&described, container) {
                return Some(described);
            }
        }

        None
    }

    fn with_id(&self, wanted: u32) -> Option<Described> {
        if wanted == 0 {
            return None;
        }

        let index = self.index();
        let checkpoint = (wanted as usize - FIRST_ID) / CHECKPOINT_SPAN;

        while index.checkpoints.len() <= checkpoint {
            self.take_in_next_type(index)?;
        }

        let mut described = self.at(index.checkpoints[checkpoint] as usize)?;
        let mut id = (checkpoint * CHECKPOINT_SPAN + FIRST_ID) as u32;
        while id != wanted {
            described = self.at(described.body + described.body_size)?;
            id += 1;
        }

        Some(described)
    }

    fn take_in_next_type(&self, index: &mut Index) -> Option<Described> {
        let described = self.at(index.offset)?;

        if (index.id as usize - FIRST_ID) % CHECKPOINT_SPAN == 0 {
            index.checkpoints.push(index.offset as u32);
        }

        if self.holds_fields(&described) && described.name != 0 {
            index
                .containers
                .insert(self.name_of(described.name), index.offset as u32);
        }

        index.offset = described.body + described.body_size;
        index.id += 1;

        Some(described)
    }

    fn names_container(&self, described: &Described, container: &str) -> bool {
        self.holds_fields(described)
            && described.name != 0
            && self.name_of(described.name) == container
    }

    fn holds_fields(&self, described: &Described) -> bool {
        described.kind == KIND_STRUCT || described.kind == KIND_UNION
    }

    fn index(&self) -> &'static mut Index {
        let known = unsafe { (&raw mut INDEX).as_mut().unwrap() };
        if known.is_none() {
            *known = Some(Index {
                containers: BTreeMap::new(),
                checkpoints: Vec::new(),
                offset: 0,
                id: FIRST_ID as u32,
            });
        }

        known.as_mut().unwrap()
    }

    fn visit_fields(
        &self,
        described: &Described,
        base: usize,
        on_field: &mut dyn FnMut(&str, &Field) -> bool,
    ) -> bool {
        for index in 0..described.members {
            let member = described.body + (index * MEMBER_SIZE);
            let name = self.name_of(word_at(self.types, member));
            let placement = base + self.placement_of(described, member);

            if name.is_empty() {
                let Some(within) = self.with_id(word_at(self.types, member + 4)) else {
                    continue;
                };
                if within.kind == KIND_STRUCT || within.kind == KIND_UNION {
                    if !self.visit_fields(&within, placement, on_field) {
                        return false;
                    }
                }
            } else {
                let field = Field {
                    offset: placement / 8,
                    bit_offset: placement,
                    bit_size: self.bits_of(described, member),
                    id: word_at(self.types, member + 4),
                };
                if !on_field(name, &field) {
                    return false;
                }
            }
        }

        true
    }

    fn function_named(&self, wanted: &str) -> Option<Described> {
        let known = self.functions();

        let at = known
            .binary_search_by(|entry| self.name_of(entry.name).cmp(wanted))
            .ok()?;

        self.at(known[at].offset as usize)
    }

    fn functions(&self) -> &'static [Named] {
        let known = unsafe { (&raw mut FUNCTIONS).as_mut().unwrap() };
        if known.is_none() {
            let mut found = Vec::new();

            self.visit_every_type_at(&mut |offset, described| {
                if described.kind == KIND_FUNCTION && described.name != 0 {
                    found.push(Named {
                        name: described.name,
                        offset: offset as u32,
                    });
                }

                true
            });

            found.sort_unstable_by(|a, b| self.name_of(a.name).cmp(self.name_of(b.name)));

            *known = Some(found);
        }

        known.as_ref().unwrap()
    }

    fn visit_parameters(&self, signature: &Described, on_parameter: &mut dyn FnMut(&str, &str)) {
        for index in 0..signature.members {
            let parameter = signature.body + (index * PARAMETER_SIZE);
            let name = self.name_of(word_at(self.types, parameter));
            let id = word_at(self.types, parameter + 4);

            if id == 0 {
                on_parameter("", "...");
                continue;
            }

            on_parameter(name, &self.render(id, 0));
        }
    }

    fn render(&self, id: u32, depth: usize) -> String {
        if depth == MAX_DEPTH {
            return String::from("...");
        }

        let Some(described) = self.with_id(id) else {
            return String::from("void");
        };
        let named = self.name_of(described.name);

        match described.kind {
            KIND_POINTER => alloc::format!("{} *", self.render(described.size, depth + 1)),
            KIND_ARRAY => {
                let element = word_at(self.types, described.body);
                let count = word_at(self.types, described.body + 8);
                alloc::format!("{}[{}]", self.render(element, depth + 1), count)
            }
            KIND_STRUCT => alloc::format!("struct {named}"),
            KIND_UNION => alloc::format!("union {named}"),
            KIND_ENUM | KIND_WIDE_ENUM => alloc::format!("enum {named}"),
            KIND_CONSTANT => alloc::format!("const {}", self.render(described.size, depth + 1)),
            KIND_VOLATILE => alloc::format!("volatile {}", self.render(described.size, depth + 1)),
            KIND_RESTRICT => alloc::format!("{} restrict", self.render(described.size, depth + 1)),
            KIND_SIGNATURE => self.render_signature(&described, depth),
            KIND_LOOKING_AHEAD => alloc::format!("struct {named}"),
            _ => String::from(named),
        }
    }

    fn render_signature(&self, described: &Described, depth: usize) -> String {
        let mut rendered = alloc::format!("{} (", self.render(described.size, depth + 1));

        for index in 0..described.members {
            let parameter = described.body + (index * PARAMETER_SIZE);
            if index != 0 {
                rendered.push_str(", ");
            }
            rendered.push_str(&self.render(word_at(self.types, parameter + 4), depth + 1));
        }

        rendered.push(')');

        rendered
    }

    fn span_of(&self, id: u32, depth: usize) -> usize {
        if depth == MAX_DEPTH {
            return 0;
        }

        let Some(described) = self.with_id(id) else {
            return 0;
        };

        match described.kind {
            KIND_POINTER => core::mem::size_of::<usize>(),
            KIND_ARRAY => {
                let element = word_at(self.types, described.body);
                let count = word_at(self.types, described.body + 8) as usize;
                self.span_of(element, depth + 1) * count
            }
            KIND_INT | KIND_STRUCT | KIND_UNION | KIND_ENUM | KIND_WIDE_ENUM | KIND_FLOAT => {
                described.size as usize
            }
            KIND_TYPEDEF | KIND_CONSTANT | KIND_VOLATILE | KIND_RESTRICT | KIND_TAGGED_TYPE => {
                self.span_of(described.size, depth + 1)
            }
            _ => 0,
        }
    }

    fn bits_of(&self, described: &Described, member: usize) -> usize {
        if !described.members_carry_bitfields {
            return 0;
        }

        (word_at(self.types, member + 8) >> 24) as usize
    }

    fn constant_named(&self, wanted: &str) -> Option<i64> {
        let known = unsafe { (&raw mut CONSTANTS).as_mut().unwrap() };
        if let Some(value) = known.get(wanted) {
            return Some(*value);
        }

        let mut found = None;
        self.visit_every_type(&mut |described| {
            if !self.holds_constants(described) {
                return true;
            }

            self.visit_constants(described, &mut |name, value| {
                if name == wanted {
                    found = Some(value);
                }
                found.is_none()
            });

            found.is_none()
        });

        if let Some(value) = found {
            known.insert(String::from(wanted), value);
        }

        found
    }

    fn enum_named(&self, wanted: &str) -> Option<Described> {
        let mut found = None;
        self.visit_every_type(&mut |described| {
            if self.holds_constants(described)
                && described.name != 0
                && self.name_of(described.name) == wanted
            {
                found = Some(described.clone());
            }

            found.is_none()
        });

        found
    }

    fn visit_constants(&self, described: &Described, on_constant: &mut dyn FnMut(&str, i64) -> bool) {
        let wide = described.kind == KIND_WIDE_ENUM;
        let stride = if wide { 12 } else { 8 };

        for index in 0..described.members {
            let constant = described.body + (index * stride);
            let name = self.name_of(word_at(self.types, constant));

            let value = if wide {
                ((word_at(self.types, constant + 8) as u64) << 32
                    | word_at(self.types, constant + 4) as u64) as i64
            } else {
                word_at(self.types, constant + 4) as i32 as i64
            };

            if !on_constant(name, value) {
                return;
            }
        }
    }

    fn visit_every_type(&self, on_type: &mut dyn FnMut(&Described) -> bool) {
        self.visit_every_type_at(&mut |_offset, described| on_type(described));
    }

    fn visit_every_type_at(&self, on_type: &mut dyn FnMut(usize, &Described) -> bool) {
        let mut offset = 0;
        while let Some(described) = self.at(offset) {
            if !on_type(offset, &described) {
                return;
            }

            offset = described.body + described.body_size;
        }
    }

    fn holds_constants(&self, described: &Described) -> bool {
        described.kind == KIND_ENUM || described.kind == KIND_WIDE_ENUM
    }

    // A structure the kernel lays out itself keeps its fields in a member with no name of its
    // own, so a field is looked for through those as well, and answers where it sits in the
    // structure the question was asked about.
    fn offset_of(&self, described: &Described, field: &str) -> Option<usize> {
        for index in 0..described.members {
            let member = described.body + (index * MEMBER_SIZE);
            let name = self.name_of(word_at(self.types, member));
            let placement = self.placement_of(described, member);

            if name == field {
                return Some(placement);
            }

            if name.is_empty() {
                let within = self.with_id(word_at(self.types, member + 4))?;
                if within.kind == KIND_STRUCT || within.kind == KIND_UNION {
                    if let Some(deeper) = self.offset_of(&within, field) {
                        return Some(placement + deeper);
                    }
                }
            }
        }

        None
    }

    fn placement_of(&self, described: &Described, member: usize) -> usize {
        let placement = word_at(self.types, member + 8);

        let bits = if described.members_carry_bitfields {
            placement & 0x00ff_ffff
        } else {
            placement
        };

        bits as usize
    }

    fn at(&self, offset: usize) -> Option<Described> {
        if offset + DESCRIPTION_SIZE > self.types.len() {
            return None;
        }

        let name = word_at(self.types, offset);
        let info = word_at(self.types, offset + 4);
        let size = word_at(self.types, offset + 8);

        let kind = (info >> 24) & 0x1f;
        let members = (info & 0xffff) as usize;

        Some(Described {
            name,
            kind,
            size,
            members,
            members_carry_bitfields: (info >> 31) != 0,
            body: offset + DESCRIPTION_SIZE,
            body_size: body_size_of(kind, members),
        })
    }

    fn name_of(&self, offset: u32) -> &'static str {
        let start = offset as usize;
        let end = self.names[start..]
            .iter()
            .position(|letter| *letter == 0)
            .map(|length| start + length)
            .unwrap_or(self.names.len());

        core::str::from_utf8(&self.names[start..end]).unwrap_or("")
    }
}

static mut CONSTANTS: BTreeMap<String, i64> = BTreeMap::new();

static mut FUNCTIONS: Option<Vec<Named>> = None;

static mut INDEX: Option<Index> = None;

static mut RESOLVED: Vec<Described> = Vec::new();

struct Named {
    name: u32,
    offset: u32,
}

struct Index {
    containers: BTreeMap<&'static str, u32>,
    checkpoints: Vec<u32>,
    offset: usize,
    id: u32,
}

#[derive(Clone)]
struct Described {
    name: u32,
    kind: u32,
    size: u32,
    members: usize,
    members_carry_bitfields: bool,
    body: usize,
    body_size: usize,
}

fn described_types() -> Option<DescribedTypes> {
    let start = unsafe { ___start_BTF } as usize;
    let stop = unsafe { ___stop_BTF } as usize;
    if start == 0 || stop <= start {
        return None;
    }

    let described = unsafe { core::slice::from_raw_parts(start as *const u8, stop - start) };
    if u16::from_ne_bytes([described[0], described[1]]) != DESCRIPTION_MAGIC {
        return None;
    }

    let header_size = word_at(described, 4) as usize;
    let types = header_size + word_at(described, 8) as usize;
    let types_size = word_at(described, 12) as usize;
    let names = header_size + word_at(described, 16) as usize;
    let names_size = word_at(described, 20) as usize;
    if names + names_size > described.len() {
        return None;
    }

    Some(DescribedTypes {
        types: &described[types..types + types_size],
        names: &described[names..names + names_size],
    })
}

fn body_size_of(kind: u32, members: usize) -> usize {
    match kind {
        KIND_INT | KIND_VARIABLE | KIND_TAG => 4,
        KIND_ARRAY => 12,
        KIND_STRUCT | KIND_UNION => members * MEMBER_SIZE,
        KIND_ENUM | KIND_SIGNATURE => members * 8,
        KIND_SECTION | KIND_WIDE_ENUM => members * 12,
        _ => 0,
    }
}

fn word_at(bytes: &[u8], offset: usize) -> u32 {
    u32::from_ne_bytes([
        bytes[offset],
        bytes[offset + 1],
        bytes[offset + 2],
        bytes[offset + 3],
    ])
}

const DESCRIPTION_MAGIC: u16 = 0xeb9f;
const DESCRIPTION_SIZE: usize = 12;
const FIRST_ID: usize = 1;
const CHECKPOINT_SPAN: usize = 64;
const MEMBER_SIZE: usize = 12;

const KIND_INT: u32 = 1;
const KIND_POINTER: u32 = 2;
const KIND_ARRAY: u32 = 3;
const KIND_STRUCT: u32 = 4;
const KIND_UNION: u32 = 5;
const KIND_ENUM: u32 = 6;
const KIND_FUNCTION: u32 = 12;
const KIND_LOOKING_AHEAD: u32 = 7;
const KIND_TYPEDEF: u32 = 8;
const KIND_VOLATILE: u32 = 9;
const KIND_CONSTANT: u32 = 10;
const KIND_RESTRICT: u32 = 11;
const KIND_SIGNATURE: u32 = 13;
const KIND_VARIABLE: u32 = 14;
const KIND_SECTION: u32 = 15;
const KIND_TAG: u32 = 17;
const KIND_FLOAT: u32 = 16;
const KIND_TAGGED_TYPE: u32 = 18;
const KIND_WIDE_ENUM: u32 = 19;
const PARAMETER_SIZE: usize = 8;
const MAX_DEPTH: usize = 8;

unsafe extern "C" {
    static ___start_BTF: *const c_void;
    static ___stop_BTF: *const c_void;
}
