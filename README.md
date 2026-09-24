# RISC-V Assembler

A RISC-V 32-bit assembler written from scratch in C. It translates RV32I+M assembly into ELF32 executables that can be run by the companion [riscv-emulator](https://github.com/Kelexer1/riscv-emulator).

Designed for Unix systems, or WSL on Windows.

## Pipeline

Source is processed in stages, each with explicit ownership of its allocations through an arena allocator:

1. **Lexer** turns source text into tokens
2. **Parser** builds instructions, directives, and labels
3. **Expander** decomposes pseudoinstructions into base instructions
4. **First pass** builds the symbol table and assigns addresses
5. **Second pass** resolves symbols and encodes the binary
6. **ELF writer** emits an ELF32 executable

## Supported Features

**Instruction sets:** RV32I and the M extension

**Pseudoinstructions:** `nop`, `mv`, `not`, `neg`, `seqz`, `snez`, `sltz`, `sgtz`, `beqz`, `bnez`, `blez`, `bgez`, `bltz`, `bgtz`, `bgt`, `ble`, `bgtu`, `bleu`, `j`, `jal`, `jr`, `jalr`, `ret`, `call`, `tail`, `li`, `la`, `lla`

**Directives:** `.text`, `.data`, `.bss`, `.rodata`, `.byte`, `.half`, `.word`, `.string`/`.asciz`, `.ascii`, `.zero`/`.space`, `.equ`/`.set`, `.align`/`.balign`/`.p2align`

## Dependencies

- cmake 3.21 or newer
- gcc 13 or newer
- clangd (optional, for editor tooling)

## Building

```sh
cmake -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release
```

Other build types: `Debug` (default, AddressSanitizer), `Test` (builds the unit tests), and `Profile`.

## Usage

```sh
riscv-assembler [-o <output>] <file.s>
```

The output defaults to `a.out`. To assemble and run a program:

```sh
riscv-assembler -o hello.elf hello.s
riscv-emulator run hello.elf
```

## Running Tests

```sh
cmake -B build/test -DCMAKE_BUILD_TYPE=Test
cmake --build build/test
ctest --test-dir build/test --output-on-failure
```

## Known Limitations

- Operands cannot contain operator expressions (for example `li x1, (1 << 4) - 1`) or `symbol+offset`
- Numeric local labels (`1:`, `1f`, `1b`) are not supported
- `fence` and `fence.i` are not supported, even though the emulator supports them