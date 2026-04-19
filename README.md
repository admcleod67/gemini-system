# pick-system

A **Pick System R83–like** environment built from the ground up in **modern C++**. It is heavily inspired by **Pick OS**
in spirit and direction, but it is **not** a port or clone: the design evolves deliberately and may include capabilities
beyond classic Pick—for example, **more direct interaction with the bytecode VM** (developer shell, stack inspection,
test hooks) to support experimentation and teaching.

The **current implementation is intentionally minimal**: a small virtual machine, a text bytecode format, a parser, and
a thin developer shell. The goal is to grow the system in **small, verifiable steps** (tests, clear boundaries) rather
than landing a large stack at once.

## Building

Requires **CMake 3.16+** and a **C++17** toolchain.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Artifacts include **`pick-system`** (minimal host entry point), **`pick-cli`** (interactive shell over the VM), and **`pick-tests`** (unit tests).

## Documentation

More detail lives under **[`docs/`](docs/README.md)**:

- **[Bytecode VM](docs/vm.md)** — `.tbc` format, opcodes, parser and runtime.
- **[Developer shell (TCL)](docs/tcl-shell.md)** — REPL commands, trace, breakpoints.

## Layout (high level)

- **`src/core/vm/`** — bytecode parser and VM runtime (`pick-core`)
- **`src/userland/tcl/`** — developer shell (`Shell`, `pick-tcl`)
- **`include/pick_system/`** — shared headers (e.g. version)
- **`include/pickvm/`** — public VM umbrella header (`core.hpp`)
- **`tests/`** — **doctest**-based tests
- **`programs/`** — sample `.tbc` bytecode files
- **`docs/`** — VM and shell documentation

## License

See [LICENSE](LICENSE).
