# CMake Toolchain Files

Cross-compilation toolchain files for building on different architectures.

## Usage

### Linux ARM64 Cross-Compilation

**Prerequisites** (on x86_64 Linux):
```bash
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

**Build**:
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-aarch64.cmake -B build-arm64
cmake --build build-arm64
```

### macOS Universal Binary

Build a single binary that runs on both Intel and Apple Silicon:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/macos-universal.cmake -B build-universal
cmake --build build-universal
```

**Note**: Requires all dependencies to be available as universal binaries.

## CI/CD Approach (Recommended)

Instead of cross-compilation, use GitHub Actions with native runners for each architecture:
- macOS Intel: `macos-13`
- macOS Apple Silicon: `macos-14`
- Linux x86_64: `ubuntu-latest`
- Linux ARM64: QEMU or native ARM runners
- Windows x64: `windows-latest`

See `.github/workflows/build-packages.yml` for the complete CI/CD setup.
