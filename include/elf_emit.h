#ifndef ELF_EMIT_H
#define ELF_EMIT_H

#include "../include/assembled_program.h"

/**
 * @brief Writes an assembled program to disk as a RISC-V ELF32 executable
 *
 * @param prog The assembled program
 * @param path Output file path
 * @return int 1 on success, 0 otherwise
 */
int write_elf_file(const AssembledProgram* prog, const char* path);

#endif