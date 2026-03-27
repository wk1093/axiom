# Axiom
**A self-contained AArch64/ELF toolchain built from the ground up.**

Axiom is a hobbyist systems programming project focused on implementing the core components of a compilation and linking pipeline. It is written in pure C with zero dependencies beyond `libc` and the standard `elf.h` headers.

## Project Structure

The project is divided into four primary components:

| Component | Description | Status |
| :--- | :--- | :--- |
| **`libax/`** | The "heart" of the project. Contains shared logic, custom vector implementations, ELF object manipulation, and common utilities. | **Stable** |
| **`axas/`** | A custom assembler that transforms assembly source into ELF object files. Supports a subset of AArch64 instructions and standard directives. | **Functional** |
| **`axld/`** | A minimalist linker. Currently capable of taking a single Axiom-generated object file and producing a valid executable. | **WIP** (Single Object) |
| **`axcc/`** | The planned C compiler for the Axiom ecosystem. | **Planned** |

## Getting Started

### Prerequisites
* A C compiler (GCC or Clang)
* GNU Make
* A Linux environment (for ELF support and AArch64 testing)

### Building
The project uses a top-level Makefile to orchestrate the build of all sub-components:

```bash
git clone https://github.com/wk1093/axiom
cd axiom
make
```

Binaries for the assembler and linker will be generated in their respective `bin/` or `build/` directories.

## Usage

### Assembling
To transform an assembly file into an ELF object:
```bash
./axas/bin/axas input.s -o output.o
```

### Linking
To create an executable from your object file:
```bash
./axld/bin/axld output.o -o my_program
```

## Current Limitations
Axiom is a work-in-progress and currently operates under the following constraints:
* **`axas`**: Only supports a specific subset of the AArch64 instruction set.
* **`axld`**: Supports linking only a **single** object file. It does not currently support static (`.a`) or dynamic (`.so`) libraries.
* **Architecture**: Primarily focused on AArch64 (ARM64) systems.

## Philosophy
* **Zero Dependencies**: No third-party libraries. If we need a data structure, we build it.
* **Transparency**: Code is written to be readable and to serve as a reference for how ELF files are structured and manipulated.
* **Low-Level Control**: Direct manipulation of section headers, symbol tables, and relocation entries.

## New Instructions

New instructions can be added by only modifying two different places:
- libax/include/ax_instr_def.h: X-macros containing main information about an instruction.
- libax/src/axas_assembler.c: If the instruction has any special rules about how it is assembled.

## TODO
- [ ] Add more instructions to assembler
- [ ] Add support for all sections (.rodata, .bss, etc.)
- [ ] Add support for multiple object files
- [ ] Add supprt for .a library files (statically linked)
- [ ] HARD: Add support for dynamically linking
- [ ] HARD: Write C compiler
