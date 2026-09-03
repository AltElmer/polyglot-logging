# Polyglot CLI Logging Reference Implementation (C / C++ / Fortran)

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Standard](https://img.shields.io/badge/C-11-blue.svg)](https://en.cppreference.com/w/c/11)
[![Standard](https://img.shields.io/badge/C%2B%2B-17-blue.svg)](https://en.cppreference.com/w/cpp/17)
[![Standard](https://img.shields.io/badge/Fortran-2008-blue.svg)](https://fortran-lang.org/)
[![NO_COLOR](https://img.shields.io/badge/NO__COLOR-compliant-success.svg)](https://no-color.org/)

An enterprise-hardened, hardware-agnostic, cross-platform reference implementation demonstrating canonical verbose debug logging best practices for command-line interfaces (CLIs) spanning **C**, **C++**, and **Fortran**.

---

## 1. Architectural Architecture & Design Principles

```
                  ┌──────────────────────────────┐
                  │         polyglot-cli         │
                  │                              │
                  │  CLI & Environment Parser   │
                  │  Crash Signal Handlers       │
                  │  Application Domain Logic    │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │     polyglot_log Facade      │
                  │                              │
                  │  C ABI (polyglot_log.h)      │
                  │  Fortran ISO_C (logging_mod) │
                  │  Caller Source Location      │
                  │  Fast-Path Enabled Checks    │
                  │  Fallback Bootstrap Sink     │
                  └──────────────┬───────────────┘
                                 │
                                 ▼
                  ┌──────────────────────────────┐
                  │    Core Backend (spdlog)     │
                  │    Permissive Root (TRACE)   │
                  └──────────────┬───────────────┘
                                 │
                   ┌─────────────┴─────────────┐
                   ▼                           ▼
          ┌─────────────────┐         ┌─────────────────┐
          │  Console Sink   │         │    File Sink    │
          │    (stderr)     │         │   (file.log)    │
          ├─────────────────┤         ├─────────────────┤
          │ Level: Dynamic  │         │ Level: TRACE    │
          │ Stripped noise  │         │ Source Location │
          │ ANSI colors     │         │ ISO-8601 UTC    │
          │ NO_COLOR aware  │         │ PID / Thread ID │
          │ Auto-detect TTY │         │ Rotating 10MBx3 │
          └─────────────────┘         └─────────────────┘
```

### Principle 1: Strict Stream Separation (Never Log to `stdout`)
- **`stdout` belongs exclusively to program data**: JSON payloads, tabular numerical output, binary streams, or pipeline payloads (`tool --verbose | jq .`).
- **`stderr` is reserved for all diagnostic logs**: Informational notices, debug traces, warnings, and fatal errors.
- **Pipeline Composability**: When output is redirected (`tool -v > results.dat`), diagnostic messages remain clearly visible on the terminal screen via `stderr` while `results.dat` remains completely unpolluted.

### Principle 2: The Dual-Sink Pattern with Independent Filtering
1. **Interactive Console Sink (`stderr`)**:
   - Filtered dynamically via `-q` (quiet), `--silent` (off), default `INFO`, `-v` (debug), `-vv` (trace), or `--log-level <LEVEL>`.
   - Clean formatting (`%^[%l]%$ %v`) using ANSI colors when attached to an interactive TTY.
   - Compliant with the [NO_COLOR standard](https://no-color.org): suppresses ANSI color escapes when `NO_COLOR` is present.
   - Suppresses microsecond timestamps, thread IDs, and source file locations to reduce human cognitive load.
2. **Forensic File Sink (`-l <path>` / `--log-file <path>`)**:
   - Forensic disk sink capturing down to `TRACE` level by default (independently configurable via `--log-file-level <LEVEL>`).
   - Plain text (ANSI escape codes strictly disabled).
   - High-precision ISO-8601 timestamps, process ID (PID), thread ID, component tag, and exact caller source locations (`[%s:%#]`).
   - Size-based rotation (10 MB per file, 3 generations) to prevent runaway disk consumption.

### Principle 3: Root-Logger Permissiveness & Accurate Facade Checks
- The central root logger is configured to `TRACE` (most permissive).
- Individual sinks filter independently.
- **Accurate Fast-Path Semantics**: `logger_is_enabled(level)` tracks the active thresholds of both the console and file sinks directly, guaranteeing that expensive numerical serialization is skipped when neither sink accepts the event.

### Principle 4: Concurrency & Lifecycle Safety
- **No Data Race on Shutdown**: `logger_shutdown()` flushes sinks and drops registration without nullifying the logger pointer, leaving a static fallback bootstrap logger active to absorb any late static destructors or teardown logs safely.
- **Initialization Error Reporting**: `logger_init(...)` returns explicit `logger_status_t` codes and automatically creates missing parent directories. If an explicitly requested file sink cannot be created, the CLI reports an error and aborts.

### Principle 5: Caller Source-Location Passthrough
- Exposes `logger_dispatch_loc(...)` accepting `file`, `line`, `func` (`spdlog::source_loc`).
- Provides ergonomic C and C++ macros (`LOG_INFO`, `LOGF_DEBUG`, `LOG_TRACE`, etc.) that automatically inject `__FILE__`, `__LINE__`, and `__func__` into disk logs without polluting console output.

### Principle 6: Decoupled Architecture
- Domain numerical routines (`run_c_computation`, `run_fortran_solver`) live in dedicated headers (`include/c_subsystem.h`, `include/fortran_solver.h`), keeping `include/polyglot_log.h` 100% pure as a reusable logging library interface.

---

## 2. Configuration Precedence

Configuration resolves according to standard Unix hierarchy:

```
Defaults  ──►  Environment Variables  ──►  CLI Arguments (Highest Precedence)
```

| Setting | Default | Environment Variable | CLI Switch |
| :--- | :--- | :--- | :--- |
| **Console Level** | `INFO` | `POLYGLOT_LOG_LEVEL` | `-v`, `-vv`, `-q`, `--silent`, `--log-level=LEVEL` |
| **File Path** | *None* | `POLYGLOT_LOG_FILE` | `-l PATH`, `--log-file PATH`, `--log-file=PATH` |
| **File Level** | `TRACE` | `POLYGLOT_LOG_FILE_LEVEL` | `--log-file-level=LEVEL` |
| **Color Mode** | `auto` | `NO_COLOR` (forces off) | `--color auto\|always\|never` |

---

## 3. Project Structure

```
.
├── .editorconfig                  # Consistent cross-editor whitespace & indentation
├── .clang-format                  # Standardized C/C++ code formatting rules
├── .gitignore                     # Git ignore rules for build, logs, and packages
├── CMakeLists.txt                 # Modern CMake 3.24+ (spdlog, CPack, CTest, examples)
├── LICENSE                        # MIT License (FLOSS)
├── README.md                      # Architecture guide and documentation
├── .github/workflows/ci.yml       # GitHub Actions CI matrix (Linux, macOS, Windows)
├── cmake/
│   └── run_cli_test.cmake         # Cross-platform CTest execution & stream assertor
├── include/
│   ├── polyglot_log.h             # Pure reusable C ABI logging facade
│   ├── c_subsystem.h              # Pure C computational component header
│   └── fortran_solver.h           # Fortran solver component header
├── src/
│   ├── polyglot_log.cpp           # C++ spdlog dual-sink core & C ABI export
│   ├── c_subsystem.c              # Pure C computational component
│   ├── logging_mod.f90            # Fortran 2008 ISO_C_BINDING module & solver routine
│   └── main.cpp                   # Production CLI entry point
├── tests/
│   └── test_multithreaded.cpp     # Multi-threaded concurrency stress test
└── examples/                      # Standalone zero-dependency MWEs
    ├── CMakeLists.txt             # Integrated example build rules
    ├── standalone_c/              # Pure C99 dual-sink minimal CLI (thread-safe)
    ├── standalone_cpp/            # Pure C++17 fold-expression minimal CLI (thread-safe)
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
# Configure build with tests and examples enabled
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DBUILD_EXAMPLES=ON

# Compile all targets
cmake --build build
```

### CLI Options

```
Usage: polyglot-cli [OPTIONS]

Options:
  -v, --verbose              Increase console verbosity (-v for DEBUG, -vv for TRACE)
  -q, --quiet                Quiet mode: display only errors and warnings on console
      --silent               Silent mode: disable all console logging (LOG_LVL_OFF)
      --log-level LEVEL      Explicitly set console level (trace|debug|info|warn|error|off)
  -l, --log-file PATH        Enable rotating file sink at specified path
      --log-file-level LEVEL Set file sink level independently (default: trace)
      --color MODE           Terminal color mode (auto|always|never)
  -h, --help                 Display this help message and exit
      --version              Display version information and exit
```

### Stream Separation in Action

```bash
# Pipeline execution: stdout payload captured to results.dat, diagnostics stay on terminal
./build/polyglot-cli -v -l logs/execution.log > results.dat
```

**Terminal (`stderr`):**
```text
[info] [CLI] Application starting
[debug] [CLI] Parsed CLI config: verbosity=1, quiet=0, silent=0, log_file='logs/execution.log'
[info] [C-Subsystem] Initializing sparse matrix allocation
[debug] [C-Subsystem] Allocated 4096 bytes for CSR row pointers
[info] [C-Subsystem] Matrix factorization complete
[info] [Fortran-Solver] Initializing Krylov subspace iterative solver
[debug] [Fortran-Solver] Iteration 1: Residual norm = 1.420E-02
[debug] [Fortran-Solver] Iteration 2: Residual norm = 8.150E-06
[info] [Fortran-Solver] Solver converged in 2 iterations (tol=1.0e-05)
[info] [CLI] Application completed successfully
```

**Output Payload (`results.dat` via `stdout`):**
```text
COMPUTATION_SUCCESS: 42
```

**Forensic Disk Log (`logs/execution.log` with Source Locations):**
```text
[2026-09-03T11:15:00.123+02:00] [18420:18420] [info] [main.cpp:165] [CLI] Application starting
[2026-09-03T11:15:00.124+02:00] [18420:18420] [debug] [main.cpp:168] [CLI] Parsed CLI config: verbosity=1, quiet=0, silent=0, log_file='logs/execution.log'
[2026-09-03T11:15:00.124+02:00] [18420:18420] [info] [c_subsystem.c:13] [C-Subsystem] Initializing sparse matrix allocation
[2026-09-03T11:15:00.124+02:00] [18420:18420] [debug] [c_subsystem.c:17] [C-Subsystem] Allocated 4096 bytes for CSR row pointers
[2026-09-03T11:15:00.125+02:00] [18420:18420] [trace] [c_subsystem.c:19] [C-Subsystem] CSR index verification passed [dim=64x64, nnz=256]
[2026-09-03T11:15:00.125+02:00] [18420:18420] [info] [c_subsystem.c:21] [C-Subsystem] Matrix factorization complete
[2026-09-03T11:15:00.125+02:00] [18420:18420] [info] [logging_mod.f90:78] [Fortran-Solver] Initializing Krylov subspace iterative solver
[2026-09-03T11:15:00.126+02:00] [18420:18420] [debug] [logging_mod.f90:81] [Fortran-Solver] Iteration 1: Residual norm = 1.420E-02
[2026-09-03T11:15:00.126+02:00] [18420:18420] [trace] [logging_mod.f90:84] [Fortran-Solver] Vector dot product <r, r> = 2.0164e-04
[2026-09-03T11:15:00.126+02:00] [18420:18420] [debug] [logging_mod.f90:87] [Fortran-Solver] Iteration 2: Residual norm = 8.150E-06
[2026-09-03T11:15:00.127+02:00] [18420:18420] [info] [logging_mod.f90:89] [Fortran-Solver] Solver converged in 2 iterations (tol=1.0e-05)
[2026-09-03T11:15:00.127+02:00] [18420:18420] [info] [main.cpp:177] [CLI] Application completed successfully
```

---

## 5. Automated Verification (CTest Suite)

All tests execute via the headless CMake runner script (`cmake/run_cli_test.cmake`), ensuring cross-platform reliability without shell redirection bugs.

```bash
ctest --test-dir build --output-on-failure
```

### Complete Test Coverage (19/19 Passing)
- `test_stdout_data_cleanliness`: Validates zero diagnostic leakage onto `stdout`.
- `test_default_verbosity_filtering`: Asserts default `INFO` emits without `DEBUG` or `TRACE`.
- `test_verbose_debug_flag`: Asserts `-v` unlocks `[debug]` logs.
- `test_trace_verbosity_flag`: Asserts `-vv` unlocks `[trace]` logs.
- `test_quiet_mode_flag`: Asserts `-q` suppresses `INFO`, `DEBUG`, and `TRACE`.
- `test_silent_mode_flag`: Asserts `--silent` suppresses 100% of console output.
- `test_named_log_level_flag`: Asserts `--log-level debug` explicitly controls threshold.
- `test_dual_sink_file_generation`: Verifies ISO timestamps, process/thread IDs, and disk persistence.
- `test_file_level_independence`: Verifies `--log-file-level info` suppresses disk trace logs independently.
- `test_env_var_override`: Validates `POLYGLOT_LOG_LEVEL` environment variable control.
- `test_no_color_compliance`: Asserts `NO_COLOR=1` suppresses ANSI color escapes.
- `test_conflicting_flags_error`: Validates mutual exclusion (`-q` + `-v` produces exit code 1).
- `test_help_flag`: Asserts `--help` emits usage cleanly and exits 0.
- `test_polyglot_solver_logging`: Confirms C and Fortran subsystems both log through the unified facade.
- `test_multithreaded_concurrency`: Stress-tests 8 concurrent threads logging under high contention.
- `test_standalone_c`: Asserts standalone C99 stream isolation.
- `test_standalone_cpp`: Asserts standalone C++17 stream isolation.
- `test_standalone_fortran_modern`: Asserts standalone Fortran 2008 stream isolation.
- `test_standalone_fortran_legacy`: Asserts standalone FORTRAN 77 stream isolation.

---

## 6. Packaging with CPack

Generate clean, stripped release packages with standard `GNUInstallDirs` directory hierarchies:

```bash
cd build
cpack -G "ZIP;TGZ"
```

---

## 7. License

This project is licensed under the **MIT License** — see [LICENSE](LICENSE) for details.
Free and Open Source Software (FLOSS).
