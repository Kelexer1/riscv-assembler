#ifndef FIRST_PASS_H
#define FIRST_PASS_H

#include "expander.h"
#include "symbol_table.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Generates a symbol table for an expanded program, assigning addresses to labels and
 * resolving equ constants
 *
 * @param expanded The expanded program to build a symbol table for
 * @return SymbolTable* The resulting symbol table, or NULL if an error occurred
 */
SymbolTable* assemble_symbol_table(ExpandedInput* expanded);

#endif