# Contributing to polyglot-logging

Thank you for your interest in contributing! This document details the specific practices, toolchains, testing protocols, and design principles we follow. Please review it before submitting a Pull Request.

## Design Principles

To ensure this project remains a canonical reference for robust CLI logging across C, C++, and Fortran, all contributions must adhere to the following architectural design principles:

1. **Strict Stream Separation (Never Log to `stdout`)**:
   - `stdout` belongs exclusively to program data (e.g., JSON payloads, tabular output).
   - `stderr` is strictly reserved for all diagnostic logs.
   - This ensures pipeline composability (`tool -v > results.dat`).
2. **The Dual-Sink Pattern with Independent Filtering**:
   - **Interactive Console Sink (`stderr`)**: Configurable verbosity, clean formatting, NO_COLOR aware. Suppresses noisy metadata by default.
   - **Forensic File Sink**: Captures down to `TRACE` by default, plain text or NDJSON, includes precision timestamps, PID, thread ID, and caller source locations. Rotates files based on size to prevent runaway disk consumption.
3. **Root-Logger Permissiveness & Accurate Facade Checks**:
   - The central logger is permissive (`TRACE`). Individual sinks filter independently.
   - Use `logger_is_enabled(level)` to skip expensive string/numerical operations if the log event won't be recorded.
4. **Concurrency & Lifecycle Safety**:
   - Manage the global logger atomically to guarantee memory safety under heavy multi-threaded contention.
   - Utilize a bootstrap fallback logger for safe pre-init/post-shutdown logging.
5. **Caller Source-Location Passthrough**:
   - Always propagate caller source locations using macros in C/C++ (`__FILE__`, `__LINE__`, `__func__`) and Fortran (`-cpp` / `/fpp`).
6. **Type-Safe C++ Interface**:
   - Use the C++17 fold-expression stream wrapper (`polyglot_log.hpp`) for type-safe logging in C++ rather than generic C APIs where possible.
7. **Subsystem Error Propagation**:
   - Numerical/compute subsystems should return integer status codes, which the CLI driver checks and logs before exiting non-zero if failures occur.

## Toolchains and CI matrix

Our Continuous Integration matrix verifies behavior across multiple OSes, architectures, and compilers. Before submitting a PR, ensure your changes will pass on:

- **GCC Toolchain**: GCC 11+ and GFortran 11+ on Linux, macOS, and Windows (via MSYS2 UCRT64).
- **LLVM / Clang**: Clang 16+ with GFortran on Linux and macOS (arm64).
- **Intel oneAPI**: Intel `icx`/`icpx` and Intel Fortran `ifx` on Linux and Windows.
- **Microsoft Visual Studio**: MSVC 2019/2022 with Intel Fortran `ifx` on Windows.
- **MSYS2 UCRT64**: Windows standalone portable binaries built with GCC 15/GFortran 15.

## Building and Testing

We provide a robust suite of 26 CTest cases that validate everything from multithreaded logging to JSON escaping, stream isolation in standalone executables, flag handling, and output cleanliness.

To build the project and run the tests:

```bash
# Configure build with tests and examples enabled
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DBUILD_EXAMPLES=ON

# Compile all targets
cmake --build build

# Run the 26 CTest cases
ctest --test-dir build --output-on-failure
```

If you modify functionality, you must ensure that all 26 tests still pass, or update/add tests to cover the modified behavior appropriately.

## Code Formatting

The project enforces specific formatting styles. We provide standard configuration files in the repository root:

- **`.clang-format`**: Based on the Google style guide with `IndentWidth: 4` and specific bracing and alignment rules for C/C++ code. Ensure you format your code accordingly before committing.
- **`.editorconfig`**: Controls consistent whitespace styling (e.g., indent sizes, final newlines, charset) for all languages in the repository including Fortran, CMake, and YAML. Please ensure your IDE or text editor supports and enforces these settings.
