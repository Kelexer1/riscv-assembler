#ifndef ELF32_WRITE_H
#define ELF32_WRITE_H

#include <stddef.h>
#include <stdint.h>

#define ELF32_OUT_ABS UINT16_MAX

#define PAGE_SIZE 0x1000u

typedef struct {
  const char* name;
  uint32_t vaddr;
  const uint8_t* data;
  uint32_t size;
  uint32_t flags;
} Elf32OutSection;

typedef struct {
  const char* name;
  uint32_t value;
  uint32_t size;
  uint16_t section;
  uint8_t is_global;
  uint8_t is_func;
} Elf32OutSymbol;

typedef struct {
  uint32_t entry;
  const Elf32OutSection* sections;
  size_t nsections;
  const Elf32OutSymbol* symbols;
  size_t nsymbols;
} Elf32OutImage;

/**
 * @brief Writes an elf file to a given path
 *
 * @param path The path to write to
 * @param img The image to write
 * @return int 1 if successful, 0 otherwise
 */
int elf32_write(const char* path, const Elf32OutImage* img);

#endif