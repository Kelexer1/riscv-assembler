#include "../include/elf_emit.h"
#include "../include/elf32.h"
#include "../include/elf32_write.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ELF_PAGE_SIZE 0x1000u
#define ELF_MAX_SEGMENT_SIZE 0x10000000u

enum { SLOT_TEXT, SLOT_RODATA, SLOT_DATA, SLOT_BSS, SLOT_COUNT };

static uint32_t align_page(uint32_t v) { return (v + ELF_PAGE_SIZE - 1u) & ~(ELF_PAGE_SIZE - 1u); }

static int section_slot(Section section) {
  switch (section) {
  case SECTION_TEXT:
    return SLOT_TEXT;
  case SECTION_RODATA:
    return SLOT_RODATA;
  case SECTION_DATA:
    return SLOT_DATA;
  case SECTION_BSS:
    return SLOT_BSS;
  default:
    return -1;
  }
}

int write_elf_file(const AssembledProgram* prog, const char* path) {
  if (!prog || !path)
    return 0;

  const MemorySegment* segs[SLOT_COUNT] = {&prog->text, &prog->rodata, &prog->data, &prog->bss};
  static const char* const names[SLOT_COUNT] = {".text", ".rodata", ".data", ".bss"};
  static const uint32_t flags[SLOT_COUNT] = {PF_R | PF_X, PF_R, PF_R | PF_W, PF_R | PF_W};

  uint32_t bases[SLOT_COUNT];
  uint32_t next_base = 0;
  for (int i = 0; i < SLOT_COUNT; i++) {
    if (segs[i]->size > ELF_MAX_SEGMENT_SIZE)
      return 0;
    if (i != SLOT_BSS && segs[i]->size > 0 && !segs[i]->data)
      return 0;
    bases[i] = next_base;
    next_base = align_page(next_base + (uint32_t)segs[i]->size);
  }

  Elf32OutSection sections[SLOT_COUNT];
  int slot_index[SLOT_COUNT];
  size_t nsections = 0;
  for (int i = 0; i < SLOT_COUNT; i++) {
    if (segs[i]->size == 0) {
      slot_index[i] = -1;
      continue;
    }
    sections[nsections].name = names[i];
    sections[nsections].vaddr = bases[i];
    sections[nsections].data = (i == SLOT_BSS) ? NULL : segs[i]->data;
    sections[nsections].size = (uint32_t)segs[i]->size;
    sections[nsections].flags = flags[i];
    slot_index[i] = (int)nsections++;
  }

  const SymbolTable* table = prog->symbol_table;
  size_t count = 0;
  size_t pool_size = 0;
  for (const Symbol* s = table ? table->head : NULL; s; s = s->next) {
    if (!s->name || s->len == 0)
      continue;
    count++;
    pool_size += s->len + 1;
  }

  Elf32OutSymbol* symbols = calloc(count ? count : 1, sizeof *symbols);
  char* pool = malloc(pool_size ? pool_size : 1);
  if (!symbols || !pool) {
    free(symbols);
    free(pool);
    return 0;
  }

  size_t k = 0;
  char* pool_cursor = pool;
  for (const Symbol* s = table ? table->head : NULL; s; s = s->next) {
    if (!s->name || s->len == 0)
      continue;

    memcpy(pool_cursor, s->name, s->len);
    pool_cursor[s->len] = '\0';

    Elf32OutSymbol* out = &symbols[k++];
    out->name = pool_cursor;
    out->is_global = 1;
    pool_cursor += s->len + 1;

    int slot = section_slot(s->section);
    if (slot >= 0) {
      out->value = bases[slot] + s->value;
      out->section = slot_index[slot] >= 0 ? (uint16_t)slot_index[slot] : ELF32_OUT_ABS;
    } else {
      out->value = s->value;
      out->section = ELF32_OUT_ABS;
    }
  }

  Elf32OutImage image = {
      .entry = bases[SLOT_TEXT] + prog->entry_offset,
      .sections = sections,
      .nsections = nsections,
      .symbols = symbols,
      .nsymbols = k,
  };
  int rc = elf32_write(path, &image);

  free(symbols);
  free(pool);
  return rc;
}