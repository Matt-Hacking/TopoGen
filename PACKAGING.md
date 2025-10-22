# Distribution Packaging Guide

This document describes how to create distribution packages for the Topographic Generator across macOS, Windows, and Linux platforms.

## Prerequisites

### Git Repository Setup

The packaging system is designed to work with Git for version control and source distribution:

```bash
# Initialize repository (first time only)
git init
git add .
git commit -m "Initial commit"

# Tag current version
git tag v0.22.001
```

See [Git Setup](#git-setup) section below for detailed instructions.

### Required Tools

**All Platforms:**
- Git (for source tarballs and version tagging)
- CMake 3.20+
- C++20 compiler

**macOS:**
- Xcode Command Line Tools
- Homebrew package manager
- `dylibbundler` (optional, for dependency bundling)
- `macdeployqt` (for GUI Qt bundling)

**Windows:**
- MinGW-w64 or MSVC compiler
- `windeployqt` (for GUI Qt bundling)
- vcpkg or MSYS2 for dependencies

**Linux:**
- `patchelf` (for RPATH manipulation)
- `linuxdeployqt` (optional, for GUI Qt bundling)

---

## Quick Reference

### Create Source Package

```bash
./scripts/deploy.sh --source
```

Creates a Git-based source tarball in `dist/source/`.

### Create Binary Package

```bash
# For current platform with dependencies
./scripts/deploy.sh --binary --bundle-deps

# For specific platform
./scripts/deploy.sh --platform macos --binary --bundle-deps
```

Creates platform-specific binary package in `dist/{platform}-{arch}/`.

### Create Version Archive

```bash
# Auto-increment revision
./scripts/create_version.sh "bug_fixes"

# Specify version
./scripts/create_version.sh "feature_release" --version 0.23.0
```

Creates versioned archive in `version_control/` and Git tag.

---

## Packaging Scripts

### 1. deploy.sh - Master Orchestrator

The main deployment script handles both source and binary package creation.

**Options:**
- `--source` - Create source tarball (uses git archive)
- `--binary` - Create binary package
- `--all` - Create both source and binary
- `--platform [macos|linux|windows]` - Target platform
- `--bundle-deps` - Bundle dependencies (default)
- `--no-bundle-deps` - Skip dependency bundling
- `--clean-build` - Clean and rebuild
- `--output-dir DIR` - Output directory (default: dist/)

**Examples:**

```bash
# Source only
./scripts/deploy.sh --source

# Binary with dependencies for current platform
./scripts/deploy.sh --binary --bundle-deps

# Complete package set
./scripts/deploy.sh --all --clean-build

# Cross-platform (if build system supports it)
./scripts/deploy.sh --platform linux --binary
```

**Output Structure:**

```
dist/
├── source/
│   ├── topo-gen-0.22.001-source-20250121_143000.tar.gz
│   └── version.json
├── macos-arm64/
│   ├── topo-gen-0.22.001-macos-arm64-20250121_143000.tar.gz
│   └── version.json
├── windows-x64/
│   ├── topo-gen-0.22.001-windows-x64-20250121_143000.zip
│   └── version.json
└── linux-x64/
    ├── topo-gen-0.22.001-linux-x64-20250121_143000.tar.gz
    └── version.json
```

### 2. create_version.sh - Version Control

Creates versioned archives with Git integration for release management.

**Options:**
- `[description]` - Short description (required)
- `--version MM.mm.rrr` - Specify version (optional, auto-increments if not provided)

**Examples:**

```bash
# Auto-increment revision from current version
./scripts/create_version.sh "performance_optimizations"

# Specify exact version
./scripts/create_version.sh "major_release" --version 1.0.0

# Beta release
./scripts/create_version.sh "public_beta"
```

**What It Does:**
1. Updates version in `CMakeLists.txt`
2. Creates version notes in `version_control/`
3. Creates archive using git archive (or fallback to zip)
4. Creates Git tag `v{version}`
5. Provides Git workflow reminders

**Output:**
- Archive: `version_control/cpp-topographic-generator_v{version}_{description}_{timestamp}.tar.gz`
- Notes: `version_control/cpp-topographic-generator_v{version}_{description}_notes.md`
- Git tag: `v{version}`

### 3. Dependency Bundling Scripts

Platform-specific scripts that bundle runtime dependencies with executables.

#### macOS: packaging/macos/bundle_deps.sh

Bundles Homebrew libraries and Qt dependencies.

```bash
# Bundle CLI executable
./packaging/macos/bundle_deps.sh --cli build/topo-gen

# Bundle GUI app bundle
./packaging/macos/bundle_deps.sh --gui build/topo-gen-gui.app

# Specify output directory
./packaging/macos/bundle_deps.sh --cli build/topo-gen --output-dir dist/macos-arm64
```

**Features:**
- Uses `dylibbundler` or `install_name_tool`
- Fixes @rpath to @executable_path
- Bundles GDAL data files
- Runs `macdeployqt` for GUI Qt dependencies

#### Windows: packaging/windows/bundle_deps.sh

Bundles DLLs for Windows portable distribution.

```bash
# Using vcpkg
export VCPKG_ROOT=/path/to/vcpkg
./packaging/windows/bundle_deps.sh --cli build/topo-gen.exe

# Using MSYS2
export MINGW_PREFIX=/mingw64
./packaging/windows/bundle_deps.sh --gui build/topo-gen-gui.exe

# Manual dependency directory
./packaging/windows/bundle_deps.sh --deps-dir /c/dependencies/bin --cli build/topo-gen.exe
```

**Features:**
- Auto-detects vcpkg or MSYS2 dependencies
- Runs `windeployqt` for GUI Qt dependencies
- Bundles GDAL data files
- Creates launcher script for GDAL_DATA

#### Linux: packaging/linux/bundle_deps.sh

Bundles shared libraries for Linux distribution.

```bash
# Standard bundling (for DEB/RPM)
./packaging/linux/bundle_deps.sh --cli build/topo-gen

# Portable bundle with RPATH (for AppImage)
./packaging/linux/bundle_deps.sh --portable --use-rpath --cli build/topo-gen

# GUI with Qt
./packaging/linux/bundle_deps.sh --gui --use-rpath build/topo-gen-gui
```

**Features:**
- Uses `ldd` to detect dependencies
- Sets RPATH with `patchelf`
- Runs `linuxdeployqt` for GUI Qt dependencies (optional)
- Bundles GDAL data files
- Creates launcher script

---

## Git Setup

### First-Time Repository Initialization

```bash
# 1. Initialize repository
git init

# 2. Add all files (respects .gitignore)
git add .

# 3. Create initial commit
git commit -m "Initial commit: Topographic Generator v0.22.001"

# 4. Tag current version
git tag v0.22.001 -m "Current working version"

# 5. (Optional) Add GitHub remote
git remote add origin https://github.com/username/topo-gen.git
git push -u origin main
git push --tags
```

### Daily Git Workflow

```bash
# 1. Check status
git status

# 2. Add changes
git add -A

# 3. Commit
git commit -m "Description of changes"

# 4. (Optional) Push to remote
git push
```

### Creating Releases

```bash
# 1. Use create_version.sh to create versioned release
./scripts/create_version.sh "release_description"

# 2. Push tag to remote
git push origin v0.23.0

# 3. (Optional) Create GitHub release
gh release create v0.23.0 \
    dist/source/topo-gen-0.23.0-source-*.tar.gz \
    --title "Topographic Generator v0.23.0" \
    --notes "Release notes here"
```

---

## Version Management

### Version Format

Versions follow the format `MM.mm.rrr`:
- `MM` - Major version (0, 1, 2, ...)
- `mm` - Minor version (01, 22, 23, ...)
- `rrr` - Revision (001, 002, ..., 123, ...)

### Version Locations

The version is stored in:
1. **CMakeLists.txt** - `project(TopographicGenerator VERSION 0.22.001)`
2. **Git tags** - `v0.22.001`

### Incrementing Versions

**Auto-increment revision:**
```bash
./scripts/create_version.sh "description"
# 0.22.001 → 0.22.002
```

**Specify version:**
```bash
./scripts/create_version.sh "description" --version 0.23.0
# Manual version bump
```

---

## Package Contents

### Source Package

**Included:**
- All source code (`src/`, `include/`)
- External dependencies (`external/`)
- Build scripts (`scripts/`)
- CMake configuration
- Documentation
- License files

**Excluded:**
- Build artifacts (`build/`)
- Test outputs (`test/output/`)
- Cache data (`cache/`)
- Version archives (`version_control/*.zip`)
- Private development files (`CLAUDE.md`, `CLAUDE_NOTES/`)

### Binary Package

**Structure:**
```
topo-gen-0.22.001-macos-arm64-{timestamp}/
├── bin/
│   └── topo-gen              # CLI executable
├── lib/                       # Bundled libraries (if --bundle-deps)
│   ├── libgdal.*.dylib
│   ├── libCGAL.*.dylib
│   └── ...
├── share/
│   └── gdal/                  # GDAL data files
├── topo-gen-gui.app/         # macOS GUI (if built)
├── LICENSE
├── COPYRIGHT
└── README.md
```

**Windows structure:**
- `bin/` contains `.exe` files
- `lib/` contains `.dll` files
- Includes `topo-gen-launcher.bat` for GDAL_DATA

**Linux structure:**
- `bin/` contains executables
- `lib/` contains `.so` files
- Includes `topo-gen-launcher.sh` for GDAL_DATA and LD_LIBRARY_PATH

---

## Dependency Information

See `packaging/common/dependencies.md` for comprehensive dependency documentation including:
- Required versions
- Platform-specific locations
- Bundling strategies
- License compatibility

**Core Dependencies:**
- GDAL 3.0+ (geospatial processing)
- CGAL 5.6+ (computational geometry)
- Eigen 3.4+ (linear algebra)
- Intel TBB 2021+ (parallel processing)
- Qt6 6.5+ (GUI only)

---

## Version Manifest (version.json)

Each package includes a `version.json` manifest with metadata:

```json
{
  "name": "Topographic Generator",
  "version": "0.22.001",
  "build": {
    "date": "2025-01-21T20:30:00Z",
    "commit": "a1b2c3d",
    "tag": "v0.22.001",
    "platform": "macos",
    "architecture": "arm64",
    "type": "Release"
  },
  "dependencies": {
    "gdal": "3.8.3",
    "cgal": "6.0",
    ...
  },
  "checksums": {
    "sha256": "...",
    "md5": "..."
  },
  "package": {
    "format": "binary-macos",
    "filename": "topo-gen-0.22.001-macos-arm64-20250121_203000.tar.gz",
    "size_bytes": "45678901"
  }
}
```

---

## Troubleshooting

### Git Archive Fails

If `git archive` fails, deploy.sh automatically falls back to manual tar creation.

**Common issues:**
- Not in a Git repository: Run `git init`
- Uncommitted changes: Files must be committed to be archived
- .gitignore excludes needed files: Use `git add -f` to force-add

### Dependency Bundling Issues

**macOS:**
- Install dylibbundler: `brew install dylibbundler`
- Ensure Homebrew packages are installed
- Check library paths with: `otool -L build/topo-gen`

**Windows:**
- Set `VCPKG_ROOT` or `MINGW_PREFIX` environment variables
- Ensure all DLLs are in the dependencies directory
- Check dependencies with: `ldd build/topo-gen.exe` (MinGW) or `dumpbin /dependents`

**Linux:**
- Install patchelf: `sudo apt install patchelf`
- Use `ldd build/topo-gen` to check dependencies
- In portable mode, bundle all non-system libraries

### Missing GDAL Data

If coordinate transformations fail:
- Verify GDAL data was bundled: `ls dist/.../share/gdal`
- Use launcher scripts that set `GDAL_DATA`
- Manually set: `export GDAL_DATA=/path/to/gdal`

---

## Future Enhancements

Planned for later phases (see `CLAUDE_NOTES/distribution_strategy.md`):

- **Phase 2**: macOS DMG, PKG, Homebrew formula
- **Phase 3**: Windows MSI installer
- **Phase 4**: Linux DEB, RPM, Flatpak packages
- **CI/CD**: GitHub Actions automated builds
- **Code Signing**: macOS notarization, Windows certificates

---

## References

- Distribution Strategy: `CLAUDE_NOTES/distribution_strategy.md`
- Dependency Details: `packaging/common/dependencies.md`
- Build Instructions: `README.md`
- License: `LICENSE` (MIT)
