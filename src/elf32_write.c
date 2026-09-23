#include "../include/elf32_write.h"
#include "../include/elf32.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  char* buf;
  size_t len;
  size_t cap;
} StrTab;

typedef struct {
  FILE* f;
  uint64_t pos;
  int err;
} Out;

static int strtab_init(StrTab* t) {
  t->cap = 64;
  t->len = 1;
  t->buf = calloc(1, t->cap);
  return t->buf ? 0 : -1;
}

static int strtab_add(StrTab* t, const char* s, uint32_t* off) {
  size_t n = strlen(s) + 1;
  if (t->len + n > t->cap) {
    size_t cap = t->cap;
    while (t->len + n > cap)
      cap *= 2;
    char* p = realloc(t->buf, cap);
    if (!p)
      return -1;
    t->buf = p;
    t->cap = cap;
  }
  *off = (uint32_t)t->len;
  memcpy(t->buf + t->len, s, n);
  t->len += n;
  return 0;
}

static uint64_t align_up(uint64_t v, uint64_t a) { return (v + a - 1) & ~(a - 1); }

static uint64_t congruent_offset(uint64_t cur, uint32_t vaddr) {
  uint64_t off = (cur & ~(uint64_t)(PAGE_SIZE - 1)) + (vaddr & (PAGE_SIZE - 1));
  if (off < cur)
    off += PAGE_SIZE;
  return off;
}

static void out_pad(Out* o, uint64_t target) {
  static const uint8_t zeros[256];
  while (!o->err && o->pos < target) {
    uint64_t left = target - o->pos;
    size_t n = left < sizeof zeros ? (size_t)left : sizeof zeros;
    if (fwrite(zeros, 1, n, o->f) != n)
      o->err = 1;
    o->pos += n;
  }
}

static void out_write(Out* o, const void* p, size_t n) {
  if (o->err || n == 0)
    return;
  if (fwrite(p, 1, n, o->f) != n)
    o->err = 1;
  o->pos += n;
}

int elf32_write(const char* path, const Elf32OutImage* img) {
  const size_t ns = img->nsections;
  const size_t nsym = img->nsymbols;
  const size_t shnum = ns + 4;
  const size_t symtab_idx = ns + 1;
  const size_t strtab_idx = ns + 2;
  const size_t shstrtab_idx = ns + 3;

  int rc = 0;
  int opened = 0;
  StrTab shstr = {0};
  StrTab str = {0};
  Elf32_Ehdr eh = {0};
  Elf32_Phdr* ph = NULL;
  Elf32_Shdr* sh = NULL;
  Elf32_Sym* syms = NULL;
  Out o = {0};
  uint64_t cur, symtab_off, symtab_size, strtab_off, shstrtab_off, shoff;
  size_t si = 1;
  size_t first_global = 1;

  if (ns == 0 || ns > 0xFE00 || nsym >= 0x0FFFFFFF)
    return 0;

  ph = calloc(ns, sizeof *ph);
  sh = calloc(shnum, sizeof *sh);
  syms = calloc(nsym + 1, sizeof *syms);
  if (!ph || !sh || !syms || strtab_init(&shstr) || strtab_init(&str))
    goto done;

  for (size_t i = 0; i < ns; i++) {
    const Elf32OutSection* s = &img->sections[i];
    if (!s->name || strtab_add(&shstr, s->name, &sh[i + 1].sh_name))
      goto done;
    if ((uint64_t)s->vaddr + s->size > 0x100000000ULL)
      goto done;
  }
  if (strtab_add(&shstr, ".symtab", &sh[symtab_idx].sh_name))
    goto done;
  if (strtab_add(&shstr, ".strtab", &sh[strtab_idx].sh_name))
    goto done;
  if (strtab_add(&shstr, ".shstrtab", &sh[shstrtab_idx].sh_name))
    goto done;

  for (int pass = 0; pass < 2; pass++) {
    for (size_t i = 0; i < nsym; i++) {
      const Elf32OutSymbol* s = &img->symbols[i];
      if ((s->is_global ? 1 : 0) != pass)
        continue;
      if (!s->name)
        goto done;
      if (s->section != ELF32_OUT_ABS && s->section >= ns)
        goto done;
      Elf32_Sym* e = &syms[si++];
      if (strtab_add(&str, s->name, &e->st_name))
        goto done;
      e->st_value = s->value;
      e->st_size = s->size;
      e->st_info = ELF32_ST_INFO(pass ? STB_GLOBAL : STB_LOCAL, s->is_func ? STT_FUNC : STT_NOTYPE);
      e->st_shndx = s->section == ELF32_OUT_ABS ? SHN_ABS : (Elf32_Half)(s->section + 1);
    }
    if (pass == 0)
      first_global = si;
  }

  cur = sizeof(Elf32_Ehdr) + (uint64_t)ns * sizeof(Elf32_Phdr);
  for (size_t i = 0; i < ns; i++) {
    const Elf32OutSection* s = &img->sections[i];
    const int has_data = s->data != NULL;
    const uint64_t off = congruent_offset(cur, s->vaddr);
    Elf32_Shdr* h = &sh[i + 1];

    ph[i].p_type = PT_LOAD;
    ph[i].p_offset = (Elf32_Off)off;
    ph[i].p_vaddr = s->vaddr;
    ph[i].p_paddr = s->vaddr;
    ph[i].p_filesz = has_data ? s->size : 0;
    ph[i].p_memsz = s->size;
    ph[i].p_flags = s->flags;
    ph[i].p_align = PAGE_SIZE;

    h->sh_type = has_data ? SHT_PROGBITS : SHT_NOBITS;
    h->sh_flags = SHF_ALLOC | ((s->flags & PF_W) ? SHF_WRITE : 0) | ((s->flags & PF_X) ? SHF_EXECINSTR : 0);
    h->sh_addr = s->vaddr;
    h->sh_offset = (Elf32_Off)off;
    h->sh_size = s->size;
    h->sh_addralign = (s->vaddr & 3) ? 1 : 4;

    if (has_data)
      cur = off + s->size;
  }

  symtab_off = align_up(cur, 4);
  symtab_size = (uint64_t)(nsym + 1) * sizeof(Elf32_Sym);
  strtab_off = symtab_off + symtab_size;
  shstrtab_off = strtab_off + str.len;
  shoff = align_up(shstrtab_off + shstr.len, 4);
  if (shoff + (uint64_t)shnum * sizeof(Elf32_Shdr) > UINT32_MAX)
    goto done;

  sh[symtab_idx].sh_type = SHT_SYMTAB;
  sh[symtab_idx].sh_offset = (Elf32_Off)symtab_off;
  sh[symtab_idx].sh_size = (Elf32_Word)symtab_size;
  sh[symtab_idx].sh_link = (Elf32_Word)strtab_idx;
  sh[symtab_idx].sh_info = (Elf32_Word)first_global;
  sh[symtab_idx].sh_addralign = 4;
  sh[symtab_idx].sh_entsize = sizeof(Elf32_Sym);

  sh[strtab_idx].sh_type = SHT_STRTAB;
  sh[strtab_idx].sh_offset = (Elf32_Off)strtab_off;
  sh[strtab_idx].sh_size = (Elf32_Word)str.len;
  sh[strtab_idx].sh_addralign = 1;

  sh[shstrtab_idx].sh_type = SHT_STRTAB;
  sh[shstrtab_idx].sh_offset = (Elf32_Off)shstrtab_off;
  sh[shstrtab_idx].sh_size = (Elf32_Word)shstr.len;
  sh[shstrtab_idx].sh_addralign = 1;

  eh.e_ident[EI_MAG0] = ELFMAG0;
  eh.e_ident[EI_MAG1] = ELFMAG1;
  eh.e_ident[EI_MAG2] = ELFMAG2;
  eh.e_ident[EI_MAG3] = ELFMAG3;
  eh.e_ident[EI_CLASS] = ELFCLASS32;
  eh.e_ident[EI_DATA] = ELFDATA2LSB;
  eh.e_ident[EI_VERSION] = EV_CURRENT;
  eh.e_ident[EI_OSABI] = ELFOSABI_NONE;
  eh.e_type = ET_EXEC;
  eh.e_machine = EM_RISCV;
  eh.e_version = EV_CURRENT;
  eh.e_entry = img->entry;
  eh.e_phoff = sizeof eh;
  eh.e_shoff = (Elf32_Off)shoff;
  eh.e_flags = 0;
  eh.e_ehsize = sizeof eh;
  eh.e_phentsize = sizeof(Elf32_Phdr);
  eh.e_phnum = (Elf32_Half)ns;
  eh.e_shentsize = sizeof(Elf32_Shdr);
  eh.e_shnum = (Elf32_Half)shnum;
  eh.e_shstrndx = (Elf32_Half)shstrtab_idx;

  o.f = fopen(path, "wb");
  if (!o.f)
    goto done;
  opened = 1;

  out_write(&o, &eh, sizeof eh);
  out_write(&o, ph, ns * sizeof *ph);
  for (size_t i = 0; i < ns; i++) {
    if (!img->sections[i].data)
      continue;
    out_pad(&o, ph[i].p_offset);
    out_write(&o, img->sections[i].data, img->sections[i].size);
  }
  out_pad(&o, symtab_off);
  out_write(&o, syms, (size_t)symtab_size);
  out_write(&o, str.buf, str.len);
  out_write(&o, shstr.buf, shstr.len);
  out_pad(&o, shoff);
  out_write(&o, sh, shnum * sizeof *sh);
  if (o.err)
    goto done;

  {
    FILE* f = o.f;
    o.f = NULL;
    if (fclose(f) != 0)
      goto done;
  }
  rc = 1;

done:
  if (o.f)
    fclose(o.f);
  if (rc == 0 && opened)
    remove(path);
  free(shstr.buf);
  free(str.buf);
  free(ph);
  free(sh);
  free(syms);
  return rc;
}