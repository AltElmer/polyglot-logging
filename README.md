# Polyglot CLI Logging MWE (C / C++ / Fortran)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Standard](https://img.shields.io/badge/C-11-blue.svg)](https://en.cppreference.com/w/c/11)
[![Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Standard](https://img.shields.io/badge/Fortran-2008-blue.svg)](https://fortran-lang.org/)

A production-grade, hardware-agnostic, cross-platform Minimal Working Example (MWE) demonstrating canonical verbose debug logging best practices for command-line interfaces (CLIs) spanning **C**, **C++**, and **Fortran**.

---

## 1. Core Architectural Principles

When engineering scientific, engineering, and systems CLIs, robust diagnostics require balancing terminal readability for human operators with deep forensic logs for offline analysis.

```
                      ┌───────────────────────┐
                      │ Log Event (C/C++/F08) │
                      └───────────┬───────────┘
                                  │
                    ┌─────────────┴─────────────┐
                    ▼                           ▼
           ┌─────────────────┐         ┌─────────────────┐
           │  Console Sink   │         │    File Sink    │
           │    (stderr)     │         │   (file.log)    │
           ├─────────────────┤         ├─────────────────┤
           │ Level: -v / -vv │         │ Level: TRACE    │
           │ Stripped noise  │         │ Source location │
           │ ANSI colors     │         │ ISO-8601 UTC    │
           │ Auto-detect TTY │         │ PID / Thread ID │
           └─────────────────┘         └─────────────────┘
```

### Principle 1: Strict Stream Separation (Never Log to `stdout`)
- **`stdout` belongs exclusively to program data**: JSON payloads, tabular numerical output, binary streams, or pipeline payloads (`tool --verbose | jq .`).
- **`stderr` is reserved for all diagnostic logs**: Informational status, debug traces, warnings, and fatal errors.
- **Pipeline Composability**: If a user redirects output (`tool -v > results.dat`), diagnostic messages remain clearly visible on the interactive terminal screen via `stderr` while `results.dat` remains completely unpolluted.

### Principle 2: The Dual-Sink Pattern
1. **Console Sink (`stderr`)**:
   - Filtered dynamically via CLI flags (`-q` for quiet, default `INFO`, `-v` for `DEBUG`, `-vv` for `TRACE`).
   - Clean, stripped formatting (`%^[%l]%$ %v`) using ANSI colors when attached to an interactive TTY.
   - Suppresses microsecond timestamps, thread IDs, and source file locations to reduce human cognitive load.
2. **File Sink (`-l <path>` / `--log-file <path>`)**:
   - Forensic disk sink capturing everything at `TRACE` or `DEBUG` level by default, regardless of console verbosity.
   - Plain text (ANSI escape codes strictly disabled).
   - High-precision ISO-8601 timestamps, process ID (PID), thread ID, component tag, and source locations.
   - Size-based rotation (10 MB per file, 3 generations) to prevent runaway disk consumption.

### Principle 3: Root-Logger Permissiveness
- The central root logger is configured to `TRACE` (most permissive).
- Individual sinks filter independently.
- *Anti-Pattern Defeated*: Setting the root logger to `INFO` by default causes the file sink to silently drop `DEBUG` and `TRACE` logs even when a log file is requested.

### Principle 4: Unified Polyglot Subsystem (Single Logging Engine)
- Do **not** run three separate logging engines (e.g. `spdlog` in C++, `stdlib_logger` in Fortran, and `fprintf` in C) within the same binary. Multiple uncoordinated engines cause split log files, interleaved buffer corruption, and race conditions.
- The C++ core hosts the central multi-sink `spdlog` engine.
- An `extern "C"` ABI is exposed (`include/polyglot_log.h`) for C and Fortran.
- Fortran interoperates seamlessly via standard `iso_c_binding` (`src/logging_mod.f90`).

---

## 2. The Four Deadly Traps Defeated

| Trap | Common Manifestation | Engineering Solution in this MWE |
| :--- | :--- | :--- |
| **1. Diagnostic Contamination** | Preaching `stderr` but instantiating `stdout_color_sink` or `printf` | Sinks are strictly bound to `stderr_color_sink_mt` and `std::cerr` / `error_unit`. |
| **2. Root Filter Truncation** | Filtering the root logger to `INFO` | Root logger set to `TRACE`; console and file sinks filter independently. |
| **3. Fortran String Over-Read** | Passing Fortran strings directly to C `const char*` | Fortran strings lack null-terminators (`\0`). `logging_mod.f90` explicitly appends `// c_null_char`. |
| **4. Macro Double-Evaluation** | `LOG_DEBUG(compute_hash(x))` running expensive functions twice | Strict single-evaluation via stack/heap intermediate buffer formatting in C ABI wrapper. |

---

## 3. Project Structure

```
.
├── CMakeLists.txt                 # Modern CMake (FetchContent spdlog, CPack, CTest)
├── LICENSE                        # MIT License (FLOSS)
├── README.md                      # This documentation
├── cmake/
│   └── run_cli_test.cmake         # Cross-platform CTest execution & stream assertor
├── include/
│   └── polyglot_log.h             # C ABI interface (C11 / C++ / Fortran)
├── src/
│   ├── polyglot_log.cpp           # C++ spdlog dual-sink core & C ABI export
│   ├── c_subsystem.c              # Pure C computational component
│   ├── logging_mod.f90            # Fortran 2008 ISO_C_BINDING module & solver routine
│   └── main.cpp                   # Production CLI entry point
└── examples/                      # Standalone zero-dependency MWEs
    ├── standalone_c/              # Pure C99 dual-sink minimal CLI
    ├── standalone_cpp/            # Pure C++17 fold-expression dual-sink CLI
    ├── standalone_fortran_modern/ # Modern Fortran 2008 (iso_fortran_env) CLI
    └── standalone_fortran_legacy/ # Legacy FORTRAN 77 (Unit 0, COMMON block) CLI
```

---

## 4. Building and Running

### Prerequisites
- CMake >= 3.24
- C11 and C++17 compliant compiler (GCC, Clang, or MSVC)
- Fortran 2008 compiler (GFortran, Intel `ifx`, or LLVM `flang`) — *optional; builds C/C++ if absent*
- Ninja or Make

### Build Steps

```bash
# Configure build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON

# Compile targets
cmake --build build
```

### CLI Options

```
Usage: polyglot-cli [OPTIONS]

Options:
  -v, --verbose        Increase console verbosity (-v for DEBUG, -vv for TRACE)
  -q, --quiet          Quiet mode: display only errors on console
  -l, --log-file PATH  Enable rotating file sink at specified path (captures TRACE)
  -h, --help           Display this help message and exit
      --version        Display version information and exit
```

### Stream Separation in Action

```bash
# 1. Pipeline test: stdout redirected to file, diagnostics visible on terminal
./build/polyglot-cli -v -l execution.log > results.dat
```

**Terminal (`stderr`):**
```text
[info] [CLI] Application starting
[debug] [CLI] Parsed CLI options: verbosity=1, quiet=false, log_file=execution.log
[info] [C-Subsystem] Initializing sparse matrix allocation
[debug] [C-Subsystem] Allocated 4096 bytes for CSR row pointers
[info] [C-Subsystem] Matrix factorization complete
[info] [Fortran-Solver] Initializing Krylov subspace iterative solver
[debug] [Fortran-Solver] Iteration 1: Residual norm = 1.42e-02
[debug] [Fortran-Solver] Iteration 2: Residual norm = 8.15e-06
[info] [Fortran-Solver] Solver converged in 2 iterations (tol=1.0e-05)
[info] [CLI] Application completed successfully
```

**Output Payload (`results.dat` via `stdout`):**
```text
COMPUTATION_SUCCESS: 42
```

**Forensic Disk Log (`execution.log` via File Sink):**
```text
[2026-09-03T11:00:00.123+02:00] [18420:18420] [info] [CLI] Application starting
[2026-09-03T11:00:00.124+02:00] [18420:18420] [debug] [CLI] Parsed CLI options: verbosity=1, quiet=false, log_file=execution.log
[2026-09-03T11:00:00.124+02:00] [18420:18420] [info] [C-Subsystem] Initializing sparse matrix allocation
[2026-09-03T11:00:00.124+02:00] [18420:18420] [debug] [C-Subsystem] Allocated 4096 bytes for CSR row pointers
[2026-09-03T11:00:00.125+02:00] [18420:18420] [trace] [C-Subsystem] CSR index verification passed [dim=64x64, nnz=256]
[2026-09-03T11:00:00.125+02:00] [18420:18420] [info] [C-Subsystem] Matrix factorization complete
[2026-09-03T11:00:00.125+02:00] [18420:18420] [info] [Fortran-Solver] Initializing Krylov subspace iterative solver
[2026-09-03T11:00:00.126+02:00] [18420:18420] [debug] [Fortran-Solver] Iteration 1: Residual norm = 1.42e-02
[2026-09-03T11:00:00.126+02:00] [18420:18420] [trace] [Fortran-Solver] Vector dot product <r, r> = 2.0164e-04
[2026-09-03T11:00:00.126+02:00] [18420:18420] [debug] [Fortran-Solver] Iteration 2: Residual norm = 8.15e-06
[2026-09-03T11:00:00.127+02:00] [18420:18420] [info] [Fortran-Solver] Solver converged in 2 iterations (tol=1.0e-05)
[2026-09-03T11:00:00.127+02:00] [18420:18420] [info] [CLI] Application completed successfully
```
*(Notice that even when `-v` was passed, the disk file still captured `[trace]` events!)*

---

## 5. Automated Verification with CTest

Standard shell redirection operators (`>`, `2>`) fail in standard CTest because CTest launches processes directly via platform APIs without a shell wrapper.

This project employs a CMake test driver (`cmake/run_cli_test.cmake`) via `cmake -P` to independently assert exit codes, STDOUT cleanliness, STDERR log levels, and file sink output.

```bash
ctest --test-dir build --output-on-failure
```

---

## 6. Packaging with CPack

Generate cross-platform distribution archives and OS-native packages:

```bash
cd build
# Create all configured platform packages (ZIP/TGZ on Windows; TGZ/TXZ/DEB on Linux)
cpack

# Or generate specific formats:
cpack -G TGZ
cpack -G ZIP
cpack -G DEB
```

---

## 7. Standalone Zero-Dependency Examples

For projects that cannot adopt external dependencies like `spdlog`, this repository includes four clean, self-contained standalone examples under `examples/`:
- **`examples/standalone_c/`**: Pure C99 dual-sink logging using `fprintf(stderr)` and `vfprintf(g_log_file)`.
- **`examples/standalone_cpp/`**: Pure C++17 type-safe dual-sink logging using variadic fold expressions.
- **`examples/standalone_fortran_modern/`**: Pure Fortran 2008 dual-sink logging using `iso_fortran_env`, `error_unit`, and `newunit`.
- **`examples/standalone_fortran_legacy/`**: Classic FORTRAN 77 fixed-form dual-sink logging using Unit 0, `COMMON /LOGCFG/`, and `IARGC/GETARG`.

---

## 8. License

This project is licensed under the **MIT License** — see [LICENSE](LICENSE) for details.
Free and Open Source Software (FLOSS).
