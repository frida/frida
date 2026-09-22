#include "elf-parser.h"

static const ElfW(Phdr) * frida_find_program_header_by_type (const ElfW(Ehdr) * ehdr, ElfW(Word) type);
static size_t frida_compute_elf_region_upper_bound (const ElfW(Ehdr) * ehdr, ElfW(Addr) address);

const ElfW(Dyn) *
frida_elf_find_dynamic_section (const ElfW(Ehdr) * ehdr)
{
  const ElfW(Phdr) * dyn;

  dyn = frida_find_program_header_by_type (ehdr, PT_DYNAMIC);

  return (void *) ehdr + dyn->p_vaddr;
}

const char *
frida_elf_query_soname (const ElfW(Ehdr) * ehdr)
{
  ElfW(Addr) soname_offset, strings_base;
  const ElfW(Phdr) * dyn;
  size_t num_entries, i;
  const ElfW(Dyn) * entries;

  soname_offset = 0;
  strings_base = 0;
  dyn = frida_find_program_header_by_type (ehdr, PT_DYNAMIC);
  num_entries = dyn->p_filesz / sizeof (ElfW(Dyn));
  entries = (void *) ehdr + dyn->p_vaddr;
  for (i = 0; i != num_entries; i++)
  {
    const ElfW(Dyn) * entry = &entries[i];

    switch (entry->d_tag)
    {
      case DT_SONAME:
        soname_offset = entry->d_un.d_ptr;
        break;
      case DT_STRTAB:
        strings_base = entry->d_un.d_ptr;
        break;
      default:
        break;
    }
  }
  if (soname_offset == 0 || strings_base == 0)
    return NULL;
  if (strings_base < (ElfW(Addr)) ehdr)
    strings_base += (ElfW(Addr)) ehdr;

  return (const char *) strings_base + soname_offset;
}

void
frida_elf_enumerate_exports (const ElfW(Ehdr) * ehdr, FridaFoundElfSymbolFunc func, void * user_data)
{
  ElfW(Addr) symbols_base, strings_base;
  size_t symbols_size, strings_size;
  const ElfW(Phdr) * dyn;
  size_t num_entries, i;
  size_t num_symbols;

  symbols_base = 0;
  strings_base = 0;
  symbols_size = 0;
  strings_size = 0;
  dyn = frida_find_program_header_by_type (ehdr, PT_DYNAMIC);
  num_entries = dyn->p_filesz / sizeof (ElfW(Dyn));
  for (i = 0; i != num_entries; i++)
  {
    ElfW(Dyn) * entry = (void *) ehdr + dyn->p_vaddr + (i * sizeof (ElfW(Dyn)));

    switch (entry->d_tag)
    {
      case DT_SYMTAB:
        symbols_base = entry->d_un.d_ptr;
        break;
      case DT_STRTAB:
        strings_base = entry->d_un.d_ptr;
        break;
      case DT_STRSZ:
        strings_size = entry->d_un.d_ptr;
        break;
      default:
        break;
    }
  }
  if (symbols_base == 0 || strings_base == 0 || strings_size == 0)
    return;
  if (symbols_base < (ElfW(Addr)) ehdr)
  {
    symbols_base += (ElfW(Addr)) ehdr;
    strings_base += (ElfW(Addr)) ehdr;
  }
  symbols_size = frida_compute_elf_region_upper_bound (ehdr, symbols_base - (ElfW(Addr)) ehdr);
  if (symbols_size == 0)
    return;
  num_symbols = symbols_size / sizeof (ElfW(Sym));

  for (i = 0; i != num_symbols; i++)
  {
    ElfW(Sym) * sym;
    bool probably_reached_end;
    FridaElfExportDetails d;

    sym = (void *) symbols_base + (i * sizeof (ElfW(Sym)));

    probably_reached_end = sym->st_name >= strings_size;
    if (probably_reached_end)
      break;

    if (sym->st_shndx == SHN_UNDEF)
      continue;

    d.type = FRIDA_ELF_ST_TYPE (sym->st_info);
    if (!(d.type == STT_FUNC || d.type == STT_OBJECT))
      continue;

    d.bind = FRIDA_ELF_ST_BIND (sym->st_info);
    if (!(d.bind == STB_GLOBAL || d.bind == STB_WEAK))
      continue;

    d.name = (char *) strings_base + sym->st_name;
    d.address = (void *) ehdr + sym->st_value;

    if (!func (&d, user_data))
      return;
  }
}

void
frida_elf_enumerate_symbols (const ElfW(Ehdr) * ehdr, void * loaded_base, FridaFoundElfSymbolFunc func, void * user_data)
{
  const ElfW(Sym) * symbols;
  size_t symbols_entsize, num_symbols;
  const char * strings;
  void * section_headers;
  size_t i;

  symbols = NULL;
  strings = NULL;
  section_headers = (void *) ehdr + ehdr->e_shoff;
  for (i = 0; i != ehdr->e_shnum; i++)
  {
    ElfW(Shdr) * shdr = section_headers + (i * ehdr->e_shentsize);

    if (shdr->sh_type == SHT_SYMTAB)
    {
      ElfW(Shdr) * strings_shdr;

      symbols = (void *) ehdr + shdr->sh_offset;
      symbols_entsize = shdr->sh_entsize;
      num_symbols = shdr->sh_size / symbols_entsize;

      strings_shdr = section_headers + (shdr->sh_link * ehdr->e_shentsize);
      strings = (char *) ehdr + strings_shdr->sh_offset;

      break;
    }
  }
  if (symbols == NULL)
    return;

  for (i = 0; i != num_symbols; i++)
  {
    const ElfW(Sym) * sym = &symbols[i];
    FridaElfExportDetails d;

    if (sym->st_shndx == SHN_UNDEF)
      continue;

    d.type = FRIDA_ELF_ST_TYPE (sym->st_info);
    if (!(d.type == STT_FUNC || d.type == STT_OBJECT))
      continue;

    d.bind = FRIDA_ELF_ST_BIND (sym->st_info);

    d.name = strings + sym->st_name;
    d.address = loaded_base + sym->st_value;

    if (!func (&d, user_data))
      return;
  }
}

static const ElfW(Phdr) *
frida_find_program_header_by_type (const ElfW(Ehdr) * ehdr, ElfW(Word) type)
{
  ElfW(Half) i;

  for (i = 0; i != ehdr->e_phnum; i++)
  {
    ElfW(Phdr) * phdr = (void *) ehdr + ehdr->e_phoff + (i * ehdr->e_phentsize);
    if (phdr->p_type == type)
      return phdr;
  }

  return NULL;
}

ElfW(Addr)
frida_elf_compute_base_from_phdrs (const ElfW(Phdr) * phdrs, ElfW(Half) phdr_size, ElfW(Half) phdr_count, size_t page_size)
{
  ElfW(Addr) base_address;
  ElfW(Half) i;
  const ElfW(Phdr) * phdr;

  base_address = 0;

  for (i = 0, phdr = phdrs;
      i != phdr_count;
      i++, phdr = (const void *) phdr + phdr_size)
  {
    if (phdr->p_type == PT_PHDR)
      base_address = (ElfW(Addr)) phdrs - phdr->p_offset;

    if (phdr->p_type == PT_LOAD && phdr->p_offset == 0)
    {
      if (base_address == 0)
        base_address = phdr->p_vaddr;
    }
  }

  if (base_address == 0)
    base_address = FRIDA_ELF_PAGE_START (phdrs, page_size);

  return base_address;
}

static size_t
frida_compute_elf_region_upper_bound (const ElfW(Ehdr) * ehdr, ElfW(Addr) address)
{
  ElfW(Half) i;

  for (i = 0; i != ehdr->e_phnum; i++)
  {
    ElfW(Phdr) * phdr = (void *) ehdr + ehdr->e_phoff + (i * ehdr->e_phentsize);
    ElfW(Addr) start = phdr->p_vaddr;
    ElfW(Addr) end = start + phdr->p_memsz;

    if (phdr->p_type == PT_LOAD && address >= start && address < end)
      return end - address;
  }

  return 0;
}
