# Topographic Generator - Dependencies

This document describes the runtime and build dependencies for the Topographic Generator across all platforms.

## Core Dependencies

### Required Runtime Dependencies

1. **GDAL** (>= 3.0)
   - Purpose: Geospatial data processing, coordinate transformations, contour generation
   - Used by: All modules (core, export, CLI, GUI)
   - Data files: Requires GDAL_DATA directory for projection definitions

2. **CGAL** (>= 5.6)
   - Purpose: Computational geometry, mesh operations (TrianglePlaneIntersector, TopographicMesh)
   - Used by: Core mesh processing only
   - Note: ContourGenerator uses GDAL, not CGAL

3. **Eigen** (>= 3.4)
   - Purpose: Linear algebra, matrix operations
   - Used by: Core algorithms, mesh processing

4. **Intel TBB** (>= 2021)
   - Purpose: Parallel processing, threading
   - Used by: Performance-critical algorithms

5. **Qt6** (>= 6.5) - GUI only
   - Purpose: Graphical user interface
   - Components: Qt6Core, Qt6Widgets, Qt6WebEngineWidgets, Qt6Network
   - Used by: topo-gen-gui executable only

### Optional Dependencies

6. **OpenMP** (recommended)
   - Purpose: Additional parallelization
   - Used by: Specific algorithms for multi-core acceleration
   - Fallback: Code works without OpenMP but may be slower

## Platform-Specific Dependencies

### macOS

**Package Manager**: Homebrew

```bash
brew install gdal cgal eigen tbb qt@6 libomp
```

**Library Locations**:
- Homebrew prefix: `/opt/homebrew` (Apple Silicon) or `/usr/local` (Intel)
- Libraries: `$HOMEBREW_PREFIX/lib/*.dylib`
- Headers: `$HOMEBREW_PREFIX/include`

**GDAL Data**:
- Location: `$(gdal-config --datadir)` typically `/opt/homebrew/share/gdal`
- Required for: Coordinate system transformations

**Bundling Strategy**:
- Use `dylibbundler` or `install_name_tool` to bundle Homebrew libraries
- Fix @rpath to @executable_path/../Frameworks
- Copy GDAL_DATA into .app bundle: `Contents/Resources/gdal/`

### Windows

**Package Manager**: vcpkg or MSYS2/MinGW

```bash
# vcpkg
vcpkg install gdal cgal eigen3 tbb qt6

# MSYS2
pacman -S mingw-w64-x86_64-gdal mingw-w64-x86_64-cgal \
          mingw-w64-x86_64-eigen3 mingw-w64-x86_64-tbb \
          mingw-w64-x86_64-qt6
```

**Library Locations**:
- vcpkg: `vcpkg_root/installed/x64-windows/bin/*.dll`
- MSYS2: `/mingw64/bin/*.dll`

**GDAL Data**:
- Location: Typically `share/gdal` relative to installation
- Must be bundled with executable

**Bundling Strategy**:
- Use `windeployqt` for Qt dependencies
- Copy all dependent DLLs to executable directory
- Use `dumpbin /dependents` or `ldd` to find all dependencies
- Bundle GDAL_DATA directory
- Set environment variable in launcher: `set GDAL_DATA=%~dp0gdal-data`

### Linux

**Package Manager**: apt (Debian/Ubuntu) or dnf (Fedora/RHEL)

```bash
# Ubuntu/Debian
sudo apt install libgdal-dev libcgal-dev libeigen3-dev \
                 libtbb-dev qt6-base-dev

# Fedora
sudo dnf install gdal-devel CGAL-devel eigen3-devel \
                 tbb-devel qt6-qtbase-devel
```

**Library Locations**:
- System libraries: `/usr/lib/x86_64-linux-gnu/*.so` (Ubuntu) or `/usr/lib64/*.so` (Fedora)
- Headers: `/usr/include`

**GDAL Data**:
- Location: `/usr/share/gdal`
- Typically installed by package manager

**Bundling Strategy**:
- **DEB/RPM**: Declare dependencies in package metadata, let package manager handle
- **AppImage/Flatpak**: Bundle all dependencies in container
- Use `linuxdeployqt` for Qt bundling
- Use `ldd` to find all .so dependencies
- For portable: Bundle libraries in `lib/` subdirectory, use `LD_LIBRARY_PATH` or `RPATH`

## Dependency Version Detection

### Build Time (CMake)

CMake find_package modules report versions:
```cmake
find_package(GDAL REQUIRED)
message(STATUS "GDAL version: ${GDAL_VERSION}")
```

### Runtime Detection

```bash
# GDAL
gdal-config --version

# CGAL (via CMake query)
cmake --find-package -DNAME=CGAL -DCOMPILER_ID=GNU -DLANGUAGE=CXX -DMODE=VERSION

# Eigen (header-only, version in Eigen/Core)
# TBB
# Qt
```

### Automated Detection Script

See `scripts/check_dependencies.sh` for comprehensive dependency checking.

## Minimum Versions

| Dependency | Minimum | Recommended | Notes |
|------------|---------|-------------|-------|
| GDAL       | 3.0     | 3.7+        | Newer versions have better error handling |
| CGAL       | 5.6     | 6.0+        | Used for mesh operations only |
| Eigen      | 3.4     | 3.4+        | Header-only library |
| TBB        | 2021.1  | 2021.11+    | API changes in 2021 release |
| Qt6        | 6.5     | 6.6+        | GUI only |
| OpenMP     | 4.5     | 5.0+        | Optional but recommended |

## Build Dependencies

Additional dependencies needed only for building:

- **CMake** (>= 3.20): Build system
- **C++20 Compiler**: GCC 10+, Clang 12+, MSVC 2019+
- **pkg-config**: Dependency detection (Linux/macOS)

## Testing Dependencies

- No additional test frameworks required
- Built-in smoke tests via `--help` flag

## Size Considerations

Approximate library sizes (bundled):

| Platform | GDAL | CGAL | Qt6 | Total (approx) |
|----------|------|------|-----|----------------|
| macOS    | 15MB | 5MB  | 45MB| ~70MB          |
| Windows  | 20MB | 8MB  | 50MB| ~85MB          |
| Linux    | 12MB | 4MB  | 40MB| ~60MB          |

**Note**: Sizes vary based on compression and which Qt modules are included.

## License Compatibility

All dependencies are compatible with MIT license distribution:

- GDAL: MIT/X (permissive)
- CGAL: GPL/LGPL (dual license, LGPL compatible with dynamic linking)
- Eigen: MPL2 (permissive)
- TBB: Apache 2.0 (permissive)
- Qt6: LGPL v3 (compatible with dynamic linking)
- OpenMP: Implementation-specific (typically permissive)

**Important**: When bundling CGAL, ensure dynamic linking to comply with LGPL.

## Future Considerations

- **Static Linking**: May reduce size but complicates LGPL compliance
- **Vendoring**: Consider vendoring header-only libraries (Eigen)
- **Minimal GDAL**: Custom GDAL build with only needed drivers could reduce size
