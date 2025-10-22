# Scripts Directory

This directory contains all build, test, and deployment scripts for the C++ Topographic Generator. All scripts are designed to work on macOS, Linux, and (where applicable) Windows systems.

---

## Quick Start

```bash
# First-time setup (interactive wizard)
./scripts/setup_environment.sh

# Or automated setup
./scripts/setup_environment.sh --auto-install --auto-build

# Quick development build
./scripts/quick-build.sh

# Run tests
./scripts/test.sh --all
```

---

## Script Categories

### 🏗️ Build Scripts

#### `quick-build.sh`
**Fast incremental builds for active development**

```bash
./scripts/quick-build.sh           # Debug build (default)
./scripts/quick-build.sh --release # Release build
```

- Intelligent incremental compilation
- Skips CMake configuration if not needed
- Optimized for mounted volumes (handles resource contention)
- **Use this for:** Day-to-day development

#### `build_macos.sh`
**Full macOS build with comprehensive environment setup**

```bash
./scripts/build_macos.sh
```

- Auto-detects and installs missing dependencies
- Configures LLVM/Clang environment
- Sets up OpenMP properly
- Creates fresh build from scratch
- **Use this for:** First-time builds, troubleshooting

---

### 🔧 Setup & Dependencies

#### `setup_environment.sh`
**Interactive setup wizard for first-time installation**

```bash
./scripts/setup_environment.sh                    # Interactive mode
./scripts/setup_environment.sh -y -b              # Fully automated
./scripts/setup_environment.sh --required-only    # Minimal install
```

**What it does:**
1. Checks all dependencies
2. Offers to install missing packages
3. Configures build environment
4. Optionally builds the project

**Use this for:** Setting up a new development machine

#### `check_dependencies.sh`
**Comprehensive dependency verification**

```bash
./scripts/check_dependencies.sh
```

**Checks for:**
- Build tools (CMake, Make, pkg-config)
- Compilers (GCC, Clang) with C++20 support
- Required libraries (CGAL, Eigen, GDAL, etc.)
- Optional libraries (TBB, OpenMP, libigl)
- Proper versions and compatibility

**Use this for:** Troubleshooting build issues

#### `install_dependencies.sh`
**Automated dependency installation**

```bash
./scripts/install_dependencies.sh                  # Full install
./scripts/install_dependencies.sh --required-only  # Minimal install
./scripts/install_dependencies.sh --dry-run        # Preview only
```

**Supports:**
- macOS (Homebrew)
- Ubuntu/Debian (apt)
- Fedora/RHEL (dnf/yum)
- Arch Linux (pacman)
- Conda (cross-platform)

**Use this for:** Automated dependency installation

#### `build_environment.sh`
**Auto-generated environment configuration**

```bash
source ./build_environment.sh
```

- Created by `install_dependencies.sh`
- Sets up PATH, CMAKE_PREFIX_PATH, PKG_CONFIG_PATH
- Configures LLVM and OpenMP paths
- **Use this for:** Manually configuring shell environment

---

### 🧪 Testing Scripts

#### `test.sh` ⭐ NEW
**Unified test runner with interactive and automated modes**

```bash
# Interactive menu-driven mode
./scripts/test.sh

# Run all tests
./scripts/test.sh --all

# Quick smoke tests
./scripts/test.sh --quick

# Run specific category
./scripts/test.sh --category layers

# Validate existing results
./scripts/test.sh --validate

# Clean test outputs
./scripts/test.sh --clean
```

**Test Categories:**
- `basic` - Basic functionality
- `layers` - Layer configuration
- `substrate` - Substrate and scaling
- `processing` - Simplification and smoothing
- `formats` - Output formats (SVG, STL, OBJ)
- `geographic` - OSM features
- `registration` - Registration marks and labels
- `units` - Unit systems (metric, imperial)
- `quality` - Quality levels and color schemes
- `json` - JSON configuration files
- `edge` - Edge cases and boundaries

**Use this for:** All testing needs (replaces manual test invocation)

#### `test_all_options.sh`
**Comprehensive test suite engine (50+ tests)**

```bash
./scripts/test_all_options.sh
```

- Tests every command-line option
- Uses cached Mount Denali data for speed
- Generates detailed logs
- Called by `test.sh` (prefer using `test.sh` instead)

**Use this for:** Usually invoked via `test.sh`, but can run directly

#### `validate_test_results.sh`
**Test output validation**

```bash
./scripts/validate_test_results.sh
```

- Validates log files
- Checks output file integrity (SVG, STL, OBJ)
- Verifies expected outputs exist
- Generates validation report

**Use this for:** Usually invoked via `test.sh --validate`

---

### 🧹 Maintenance Scripts

#### `clean.sh` ⭐ NEW
**Remove build artifacts and test outputs**

```bash
# Clean build artifacts
./scripts/clean.sh --build

# Clean test outputs
./scripts/clean.sh --test

# Clean downloaded SRTM tiles
./scripts/clean.sh --cache

# Clean generated output files
./scripts/clean.sh --output

# Clean everything
./scripts/clean.sh --all

# Dry run (preview without deleting)
./scripts/clean.sh --all --dry-run
```

**Use this for:** Cleaning up before fresh builds or releases

---

### 🚀 CI/CD Scripts

#### `ci.sh` ⭐ NEW
**Continuous integration pipeline**

```bash
# Full CI pipeline
./scripts/ci.sh

# Clean build + test
./scripts/ci.sh --clean

# Skip tests (build only)
./scripts/ci.sh --skip-tests

# Debug build
./scripts/ci.sh --debug --verbose
```

**Pipeline Stages:**
1. Check dependencies
2. Clean (optional)
3. Build project
4. Run comprehensive tests
5. Validate outputs
6. Generate CI report

**Exit Codes:**
- `0` - Success
- `1` - Dependency check failed
- `2` - Build failed
- `3` - Tests failed
- `4` - Validation failed

**Use this for:** Automated builds, CI/CD systems (GitHub Actions, GitLab CI, etc.)

---

### 📦 Version Control & Deployment

#### `create_version.sh`
**Version archiving system**

```bash
./scripts/create_version.sh "performance_optimizations"
./scripts/create_version.sh "bug_fixes_and_cleanup"
```

**Creates:**
- Versioned ZIP archive with source code
- Comprehensive version notes (markdown)
- Auto-incremented version numbers
- Detailed change documentation

**Archives include:**
- All source code (`src/`, `include/`)
- Build system (`CMakeLists.txt`)
- Scripts
- Documentation
- **Excludes:** Build artifacts, test outputs, cache

**Use this for:** Creating snapshots before major changes

#### `deploy.sh`
**Deployment package creation**

```bash
# Complete package (source + binaries)
./scripts/deploy.sh

# Source only
./scripts/deploy.sh --source-only

# Binaries only
./scripts/deploy.sh --build-only

# Include dependencies
./scripts/deploy.sh --include-deps --clean-build
```

**Creates:**
- Compressed tarball (`.tar.bz2`)
- Deployment info file
- Integrity verification

**Use this for:** Creating release packages for distribution

---

## Common Workflows

### 🆕 First-Time Setup

```bash
# Option 1: Interactive (recommended for beginners)
cd cpp-version
./scripts/setup_environment.sh

# Option 2: Automated (for CI or experienced users)
cd cpp-version
./scripts/setup_environment.sh --auto-install --auto-build
```

---

### 💻 Daily Development

```bash
# 1. Make code changes...

# 2. Quick incremental build
./scripts/quick-build.sh

# 3. Run quick tests
./scripts/test.sh --quick

# 4. Full test before committing
./scripts/test.sh --all
```

---

### 🧪 Testing Workflow

```bash
# Interactive testing (menu-driven)
./scripts/test.sh

# Or automated testing
./scripts/test.sh --all             # All tests
./scripts/test.sh --category layers # Specific category
./scripts/test.sh --validate        # Check results
```

---

### 🔄 Clean Build

```bash
# Clean everything and rebuild
./scripts/clean.sh --all
./scripts/quick-build.sh --release
```

---

### 🚀 Pre-Release Checklist

```bash
# 1. Clean build
./scripts/clean.sh --all
./scripts/build_macos.sh

# 2. Full test suite
./scripts/test.sh --all

# 3. Validate results
./scripts/test.sh --validate

# 4. Run CI pipeline
./scripts/ci.sh --clean

# 5. Create version archive
./scripts/create_version.sh "beta_release_candidate"

# 6. Create deployment package
./scripts/deploy.sh --clean-build
```

---

### 🔧 Troubleshooting

```bash
# Check what's missing
./scripts/check_dependencies.sh

# Re-install dependencies
./scripts/install_dependencies.sh

# Clean everything and start fresh
./scripts/clean.sh --all
./scripts/setup_environment.sh --auto-install --auto-build
```

---

## Script Maintenance Notes

### ✅ Production Scripts (15 total)

All scripts in this directory are production-ready and maintained for the beta release.

### 🛠️ Development Tools

Development-only scripts (profiling, debugging) have been moved to `../development_tools/` and are not maintained as part of the production release.

### 🔒 Best Practices

1. **Always use relative paths** - Scripts work from project root
2. **Check prerequisites** - Scripts verify their dependencies
3. **Provide help** - All scripts support `--help`
4. **Fail safely** - Use `set -e` for early error detection
5. **Log appropriately** - Important actions are logged
6. **Support automation** - Interactive and command-line modes

---

## Environment Variables

Scripts respect these environment variables:

- `BUILD_TYPE` - Build configuration (Debug/Release)
- `CMAKE_PREFIX_PATH` - CMake search paths
- `PKG_CONFIG_PATH` - pkg-config search paths
- `HOMEBREW_PREFIX` - Homebrew installation prefix
- `CONDA_PREFIX` - Conda environment prefix

---

## Platform Support

| Script | macOS | Linux | Windows |
|--------|-------|-------|---------|
| quick-build.sh | ✅ | ✅ | ⚠️ WSL |
| build_macos.sh | ✅ | ❌ | ❌ |
| test.sh | ✅ | ✅ | ⚠️ WSL |
| clean.sh | ✅ | ✅ | ⚠️ WSL |
| ci.sh | ✅ | ✅ | ⚠️ WSL |
| setup_environment.sh | ✅ | ✅ | ⚠️ WSL |
| check_dependencies.sh | ✅ | ✅ | ⚠️ WSL |
| install_dependencies.sh | ✅ | ✅ | ⚠️ WSL |
| create_version.sh | ✅ | ✅ | ⚠️ WSL |
| deploy.sh | ✅ | ✅ | ⚠️ WSL |

**Legend:**
- ✅ Fully supported
- ⚠️ Supported via WSL (Windows Subsystem for Linux)
- ❌ Not supported (platform-specific)

---

## Getting Help

Each script provides detailed help:

```bash
./scripts/<script-name> --help
```

For issues:
1. Check the script's `--help` output
2. Review the CI logs in `ci_logs/`
3. Check test logs in `test/option_tests/logs/`
4. Consult the main project `CLAUDE.md` documentation

---

**Last Updated:** $(date +"%Y-%m-%d")
**Total Scripts:** 15 production + 6 archived development tools
