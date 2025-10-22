# Multi-Architecture Build System

Complete guide for building and packaging across multiple architectures using CI/CD.

## Overview

The multi-architecture build system enables building distribution packages for:
- **macOS**: Intel (x86_64) and Apple Silicon (arm64)
- **Windows**: x64
- **Linux**: x86_64 and ARM64 (aarch64)

**Total**: 5 architecture combinations across 3 platforms

## Quick Start

### Single Command Build (Local)

```bash
# Build for native architecture and create all packages
./scripts/build-all.sh

# Clean build
./scripts/build-all.sh --clean

# Build only, skip packaging
./scripts/build-all.sh --skip-packages
```

### CI/CD (Automatic)

Push to GitHub and let CI/CD build for all architectures:

```bash
git tag v0.23.0
git push origin v0.23.0
```

GitHub Actions automatically:
1. Builds on 4 native architectures
2. Runs tests
3. Creates packages
4. Uploads to GitHub Releases

## Architecture Support Matrix

| Platform | Architecture | Runner | Status |
|----------|-------------|--------|--------|
| macOS | Intel (x86_64) | `macos-13` | ✅ Native |
| macOS | Apple Silicon (arm64) | `macos-14` | ✅ Native |
| Windows | x64 | `windows-latest` | ✅ Native |
| Windows | ARM64 | Cross-compile | ⚠️ Experimental |
| Linux | x86_64 | `ubuntu-latest` | ✅ Native |
| Linux | ARM64 | QEMU | ✅ Emulated |

## Build Methods

### Method 1: Native Builds (Recommended)

Build on each architecture using native hardware:

**Advantages**:
- Most reliable
- Full performance
- All features work
- Used by CI/CD

**Disadvantages**:
- Requires access to each architecture

**Usage**: GitHub Actions matrix builds

### Method 2: Cross-Compilation

Build for different architecture on current machine:

**Advantages**:
- Single build machine
- Faster for multiple targets

**Disadvantages**:
- Requires toolchains
- Complex dependency management
- May miss architecture-specific issues

**Usage**: Experimental, via toolchain files

### Method 3: Emulation (Linux ARM64)

Use QEMU to run ARM64 builds on x86_64:

**Advantages**:
- Works without ARM hardware
- Full Linux environment

**Disadvantages**:
- Slow (10-20x slower)
- Only used for releases

**Usage**: GitHub Actions for ARM64 Linux

## Local Development

### Build for Native Architecture

```bash
./scripts/build-all.sh
```

This:
1. Detects your platform/architecture
2. Builds executables
3. Bundles dependencies
4. Creates appropriate packages (DMG/PKG for macOS, MSI/WinGet/ZIP for Windows, DEB/RPM/Flatpak/AppImage for Linux)

### Cross-Compile (Advanced)

#### macOS Universal Binary

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/macos-universal.cmake -B build-universal
cmake --build build-universal
```

**Requirement**: Dependencies must be universal binaries (Homebrew bottles usually are)

#### Linux ARM64 (on x86_64)

```bash
# Install cross-compiler
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Build
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-aarch64.cmake -B build-arm64
cmake --build build-arm64
```

## CI/CD Setup

### GitHub Actions Workflow

Location: `.github/workflows/build-packages.yml`

**Triggers**:
- Push to `main` or `develop` branches
- Pull requests
- Git tags (`v*`)
- Manual workflow dispatch

**Matrix Strategy**:
```yaml
strategy:
  matrix:
    config:
      - { os: macos-13, arch: x86_64, platform: macos }
      - { os: macos-14, arch: arm64, platform: macos }
      - { os: windows-latest, arch: x64, platform: windows }
      - { os: ubuntu-latest, arch: x86_64, platform: linux }
```

**Per-Job Steps**:
1. Checkout code
2. Install dependencies (platform-specific)
3. Configure with CMake
4. Build
5. Run tests
6. Bundle dependencies
7. Create packages
8. Upload artifacts

**Release Job** (on tags):
1. Download all artifacts
2. Generate release notes
3. Create GitHub release
4. Upload all packages

### Customizing the Workflow

Edit `.github/workflows/build-packages.yml`:

**Add architecture**:
```yaml
- name: Linux ARM64
  os: ubuntu-latest
  arch: aarch64
  platform: linux
```

**Change dependencies**:
```yaml
- name: Set up dependencies (macOS)
  run: |
    brew install cmake gdal cgal eigen tbb libomp qt@6
    # Add more dependencies here
```

**Modify build options**:
```yaml
- name: Configure CMake
  run: |
    cmake -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DYOUR_OPTION=ON
```

## Package Creation

### Platform-Specific Packages

After building, packages are created automatically:

**macOS**:
```bash
./scripts/deploy.sh --all-macos
# Creates: DMG, PKG, Homebrew formula
```

**Windows**:
```bash
./scripts/deploy.sh --all-windows
# Creates: MSI (WiX sources), WinGet manifest, Portable ZIP
```

**Linux**:
```bash
./scripts/deploy.sh --all-linux
# Creates: DEB, RPM, Flatpak manifest, AppImage
```

### Architecture-Specific Package Names

Packages automatically include architecture in filenames:

- macOS: `topo-gen-0.23.0.dmg` (universal or architecture-specific)
- Windows: `topo-gen-0.23.0-windows-x64-portable.zip`
- Linux DEB: `topo-gen_0.23.0_amd64.deb` or `topo-gen_0.23.0_arm64.deb`
- Linux RPM: `topo-gen-0.23.0-1.x86_64.rpm` or `topo-gen-0.23.0-1.aarch64.rpm`
- AppImage: `TopoGen-0.23.0-x86_64.AppImage`

## Testing

### Local Testing

```bash
# Build
./scripts/build-all.sh

# Test executable
./build/topo-gen --version

# Test package (example for macOS)
open dist/macos-arm64/*.dmg

# Test package (example for Linux)
sudo dpkg -i dist/linux-x64/*.deb
topo-gen --version
sudo apt-get remove topo-gen
```

### CI/CD Testing

GitHub Actions automatically tests each build:

```yaml
- name: Run basic tests
  run: |
    ./build/topo-gen --version
```

Add more comprehensive tests as needed.

## Release Process

### Automated Release (Recommended)

```bash
# 1. Update version in CMakeLists.txt
# project(... VERSION 0.23.0)

# 2. Commit changes
git add CMakeLists.txt
git commit -m "Bump version to 0.23.0"
git push

# 3. Create and push tag
git tag v0.23.0
git push origin v0.23.0
```

GitHub Actions automatically:
- Builds all architectures
- Creates packages
- Generates release notes
- Creates GitHub release
- Uploads all packages

### Manual Release

```bash
# Build on each platform manually
# macOS Intel
./scripts/build-all.sh

# macOS Apple Silicon
./scripts/build-all.sh

# Windows
./scripts/build-all.sh

# Linux x86_64
./scripts/build-all.sh

# Create release
gh release create v0.23.0 \
    dist/**/*.{dmg,pkg,deb,rpm,msi,zip,AppImage,tar.gz} \
    --title "Release v0.23.0" \
    --notes "Release notes here"
```

## Architecture Detection

CMake automatically detects architecture:

```cmake
# In your CMakeLists.txt
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
include(DetectArchitecture)

# Use detected variables
message(STATUS "Building for ${TARGET_PLATFORM} ${TARGET_ARCHITECTURE}")
```

Variables set:
- `TARGET_PLATFORM`: macos, linux, windows
- `TARGET_ARCHITECTURE`: x86_64, arm64, etc.
- `PACKAGE_ARCHITECTURE`: amd64, arm64, x64 (package-specific)
- `RPM_ARCHITECTURE`: x86_64, aarch64 (RPM-specific)

## Troubleshooting

### GitHub Actions Fails

**Check logs**:
1. Go to Actions tab on GitHub
2. Click failed workflow
3. Expand failed step
4. Review error messages

**Common issues**:
- Missing dependencies: Update workflow to install them
- Build errors: Fix in source code
- Test failures: Fix tests or skip temporarily

### Cross-Compilation Fails

**Linux ARM64 cross-compile**:
```bash
# Verify cross-compiler installed
aarch64-linux-gnu-gcc --version

# Check dependencies available for target
dpkg --print-foreign-architectures
```

**macOS Universal Binary**:
```bash
# Check dependencies are universal
file /opt/homebrew/lib/libgdal.dylib
# Should show: Mach-O universal binary with 2 architectures
```

### Package Creation Fails

**Check dependencies bundled**:
```bash
ls -la dist/*-deps/
```

**Verify executables built**:
```bash
ls -la build/topo-gen*
file build/topo-gen  # Check architecture
```

## Best Practices

1. **Use CI/CD for releases**: Let GitHub Actions handle multi-arch builds
2. **Test locally first**: Ensure build works on your platform before pushing
3. **Version consistently**: Update version in one place (CMakeLists.txt)
4. **Tag releases**: Use semantic versioning (v1.0.0, v1.1.0, etc.)
5. **Document changes**: Keep CHANGELOG.md updated
6. **Test packages**: Install and test each package type before release

## Advanced Topics

### Custom Architectures

Add new architecture to GitHub Actions:

```yaml
- name: Linux RISC-V
  os: ubuntu-latest
  arch: riscv64
  platform: linux
  packages: --deb --rpm
```

### Parallel Builds

GitHub Actions runs all architectures in parallel automatically. Local parallel builds:

```bash
# Terminal 1
./scripts/build-all.sh

# Terminal 2 (on different machine/architecture)
./scripts/build-all.sh
```

### Caching

GitHub Actions caches dependencies:

```yaml
- name: Cache dependencies
  uses: actions/cache@v3
  with:
    path: ~/.cache
    key: ${{ runner.os }}-deps-${{ hashFiles('**/CMakeLists.txt') }}
```

## Summary

The multi-architecture build system provides:
- ✅ Single command local builds (`./scripts/build-all.sh`)
- ✅ Automatic CI/CD builds for all architectures
- ✅ Cross-compilation support (experimental)
- ✅ Architecture detection and normalization
- ✅ Platform-specific package creation
- ✅ Automated GitHub releases

For most users: **Just use CI/CD**. Push tags and let GitHub Actions build everything.

For local development: **Use `./scripts/build-all.sh`** to build for your current architecture.
