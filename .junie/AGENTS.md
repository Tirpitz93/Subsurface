# Project Guidelines for Subsurface

This document provides essential instructions, architecture overview, build and test workflows, and coding conventions for AI agents working on the Subsurface codebase.

---

## 1. Project Overview & Structure

Subsurface is an open-source, cross-platform dive logging and dive planning application. It supports importing data from various dive computers (via `libdivecomputer`), visualizing dive profiles, planning deco dives, and syncing dive logs (locally in XML/SSRF or via the Subsurface cloud storage backend).

### Key Directories

- **`core/`**: Main backend business logic written in C/C++. Contains dive data structures, dive computer parsing, deco algorithms (Bühlmann, VPM-B), gas calculations, and planner algorithms (`planner.cpp`, `profile.cpp`, `dive.h`, etc.).
- **`desktop-widgets/`**: Desktop UI components built using Qt Widgets.
- **`mobile-widgets/`**: Mobile UI components built using Qt Quick / QML (Kirigami framework).
- **`qt-models/`**: Qt item models bridging core data structures to desktop and mobile UI views (`diveplannermodel.cpp`, etc.).
- **`profile-widget/`**: Dive profile visualization widgets and graphics scene rendering (`profilescene.cpp`, etc.).
- **`backend-shared/`**: Shared helper utilities, cloud storage sync, and background workers.
- **`libdivecomputer/`**: Submodule/library used for communicating with physical dive computers.
- **`tests/`**: Unit and regression tests written using QtTest framework.
- **`scripts/`**: Build helper scripts (`build.sh`, `whitespace.pl`, etc.).
- **`Documentation/`**: User manuals, guides, and documentation.
- **`theme/`**: Web assets, HTML export templates, and print templates.

---

## 2. Build Instructions

Subsurface uses **CMake** as its build system with a wrapper script `scripts/build.sh`.

### Standard Build (from Parent Directory)

As specified in project guidelines, the build script must be invoked from the **parent directory** of the repository:

```bash
cd .. && ./subsurface/scripts/build.sh; cd subsurface
```

### Common Build Script Options

- `-desktop`: Build the desktop version (default target in `subsurface/build`).
- `-mobile`: Build the mobile version with QML UI (target in `subsurface/build-mobile`).
- `-build-with-qt6`: Configure build against Qt6.
- `-quick`: Rebuild only Subsurface and `libdivecomputer` without re-verifying other third-party dependencies.

### Incremental Builds

Once CMake has configured the build directory (`build/`), you can do fast incremental builds:

```bash
cmake --build build -- -j$(nproc)
```
or inside the build directory:
```bash
cd build && make -j$(nproc)
```

---

## 3. Testing Guidelines

Subsurface maintains unit tests to verify core calculations (dive planning, decompression, conversions, models, and file import/export).

### Running Tests

Tests are automatically built when building the desktop or mobile targets. Test executables reside in `<build directory>/tests` (e.g. `build/tests/`):

- **Run all tests via CTest**:
  ```bash
  cd build/tests && ctest
  ```

- **Run all tests with verbose output**:
  ```bash
  cd build/tests && ctest -V
  ```

- **Run a single test executable**:
  ```bash
  cd build/tests && ./<TestName>
  # Example:
  cd build/tests && ./TestDivePlannerModel
  ```

### Writing & Modifying Tests

- Test files are located in `tests/` (`.h` and `.cpp` lowercase).
- Tests use the QtTest framework (`QCOMPARE`, `QVERIFY`, `QVERIFY2`, `QEXPECT_FAIL`).
- When adding a new test suite, register it in `tests/CMakeLists.txt` following existing test definitions (e.g., `TestPlannerShared`).

---

## 4. Coding Style & Conventions

Detailed style guidelines are documented in `CODINGSTYLE.md`. Key rules include:

- **Indentation**: Tabs (width 8 spaces). Continuation lines are aligned with tabs then spaces.
- **Spacing**:
  - Keywords followed by parentheses have a space: `if (condition)`, `for (i = 0; i < n; i++)`, `while (loop)`.
  - Function calls do **not** have a space between function name and `(`: `function_call(arg1, arg2)`.
  - Binary operators should be padded with spaces: `a = b + 3;`.
- **Braces**:
  - Function and method implementations have their opening brace on column 1 of a new line.
  - Control flow blocks (`if`, `for`, `while`, `switch`) have the opening brace on the same line:
    ```cpp
    if (condition) {
    	do_something();
    } else {
    	do_other();
    }
    ```
- **Naming Conventions**:
  - **Core backend (C/C++)**: Typically uses `snake_case` for variables, structs, and functions (`dive_site`, `get_dive()`).
  - **Qt / UI layers**: Uses `camelCase` for variables and member functions, and `PascalCase` for class names (`DivePlannerModel`, `profileWidget`).
  - **Macros / Constants**: Use `ALL_CAPS` (`#define MAX_CYLINDERS 8`).
- **Formatting Script**: You can check or adjust formatting using `scripts/whitespace.pl`.

---

## 5. AI Agent Ground Rules

- **Mark AI Changes**: Clearly mark non-trivial AI-generated code blocks where appropriate (e.g. `// AI-generated`).
- **No Emojis**: Do not use emojis in code, comments, commit messages, or documentation.
- **Focused Changes**: Keep changes minimal, clean, and targeted to the issue at hand. Do not introduce unnecessary refactoring or style churn across unrelated files.
- **Verification**: Always verify compilation and run relevant test suites before completing tasks.
