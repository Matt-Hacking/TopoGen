# Build Instructions for C++ Topographic Generator

## Prerequisites

### System Requirements
- **C++20 compatible compiler**: GCC 10+, Clang 12+, or MSVC 2019+
- **CMake 3.20+**: For build system
- **Git**: For dependency management

### Required Libraries

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    cmake \
    git \
    libcgal-dev \
    libeigen3-dev \
    libgdal-dev \
    libtbb-dev \
    libomp-dev \
    libhdf5-dev \
    pkg-config
```

#### macOS with Homebrew
```bash
brew install cmake cgal eigen gdal tbb libomp hdf5 pkg-config
```

#### Windows with vcpkg
```bash
vcpkg install cgal eigen3 gdal tbb hdf5 openmp
```

### Optional Libraries (for enhanced features)

#### libigl (Advanced mesh processing)
**Note:** libigl is now automatically downloaded during the build process using CMake FetchContent. No manual installation is required.

If you prefer to use a system-installed version, you can install it manually:
```bash
# Ubuntu/Debian (optional)
sudo apt-get install libigl-dev

# macOS (optional) - Note: libigl is not available via Homebrew
# Use the automatic download instead

# Windows (optional)
vcpkg install libigl
```

## Building

### Quick Start
```bash
# Clone and enter directory
cd cpp-version

# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build (use -j$(nproc) for parallel build)
make -j$(nproc)

# The executable will be at: ./topo-gen
```

### Advanced Build Configuration

#### Debug Build
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

#### Custom Install Prefix
```bash
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local/topographic-generator
make -j$(nproc)
make install
```

#### Disable Parallel Processing
```bash
cmake .. -DENABLE_PARALLEL=OFF
make -j$(nproc)
```

#### Enable Testing
```bash
cmake .. -DENABLE_TESTING=ON
make -j$(nproc)
ctest
```

#### Enable Benchmarks
```bash
cmake .. -DBUILD_BENCHMARKS=ON
make -j$(nproc)
./benchmarks/benchmark_main
```

### Build Options Summary

| Option | Default | Description |
|--------|---------|-------------|
| `CMAKE_BUILD_TYPE` | Release | Build configuration (Debug, Release) |
| `ENABLE_PARALLEL` | ON | Enable TBB/OpenMP parallel processing |
| `ENABLE_NURBS` | ON | Enable NURBS surface generation |
| `ENABLE_TESTING` | ON | Build unit tests |
| `BUILD_BENCHMARKS` | OFF | Build performance benchmarks |

## Verification

### Test Basic Functionality
```bash
# Show help
./topo-gen --help

# Generate simple STL
./topo-gen --bounds "47.6,-122.3,47.7,-122.2" --layers 5 --formats stl
```

### Run Unit Tests (if enabled)
```bash
ctest --verbose
```

### Performance Benchmarks (if enabled)
```bash
./benchmarks/benchmark_main
```

## Common Build Issues

### CGAL Not Found
```bash
# Ensure CGAL is properly installed and CMake can find it
export CGAL_DIR=/usr/local/lib/cmake/CGAL
cmake .. -DCGAL_DIR=$CGAL_DIR
```

### TBB Issues on macOS
```bash
# If TBB linking fails on macOS:
export TBB_ROOT=/usr/local
cmake .. -DTBB_ROOT=$TBB_ROOT
```

### GDAL Version Conflicts
```bash
# Ensure you have GDAL 3.0+ installed
gdal-config --version

# If you have multiple GDAL versions:
cmake .. -DGDAL_CONFIG=/usr/local/bin/gdal-config
```

### OpenMP on macOS
```bash
# OpenMP may need explicit configuration on macOS:
export OpenMP_ROOT=/usr/local
cmake .. -DOpenMP_ROOT=$OpenMP_ROOT
```

## Docker Build (Alternative)

### Create Dockerfile
```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    build-essential cmake git \
    libcgal-dev libeigen3-dev libgdal-dev \
    libtbb-dev libomp-dev libhdf5-dev pkg-config

WORKDIR /app
COPY . .

RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc)

CMD ["./build/topo-gen", "--help"]
```

### Build and Run
```bash
docker build -t topographic-generator .
docker run --rm -v $(pwd)/output:/app/output topographic-generator \
    ./build/topo-gen --bounds "47.6,-122.3,47.7,-122.2" --formats stl
```

## Performance Tuning

### Compiler Optimizations
```bash
# Maximum optimization
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native -DNDEBUG"

# Profile-guided optimization (advanced)
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DCMAKE_CXX_FLAGS="-fprofile-generate"
make -j$(nproc)
# Run typical workload
./topo-gen --bounds "47.6,-122.3,47.7,-122.2" --formats stl
# Rebuild with profile data
cmake .. -DCMAKE_CXX_FLAGS="-fprofile-use"
make -j$(nproc)
```

### Memory and Threading
```bash
# Adjust for your system
export OMP_NUM_THREADS=8           # OpenMP threads
export TBB_NUM_THREADS=8           # TBB threads
export MALLOC_ARENA_MAX=4          # glibc malloc arenas
ulimit -s unlimited                # Stack size
```

## Cross-Platform Notes

### Windows (MinGW-w64)
```bash
# Use MinGW-w64 toolchain
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
mingw32-make -j$(nproc)
```

### Windows (Visual Studio)
```bash
# Use Visual Studio generator
cmake .. -G "Visual Studio 16 2019" -A x64
cmake --build . --config Release --parallel
```

### macOS Universal Binary
```bash
# Build for both Intel and Apple Silicon
cmake .. -DCMAKE_OSX_ARCHITECTURES="x86_64;arm64"
make -j$(nproc)
```

## Troubleshooting

### Build Fails with "C++20 not supported"
- Update your compiler: GCC 10+, Clang 12+, or MSVC 2019+
- Explicitly set C++ standard: `cmake .. -DCMAKE_CXX_STANDARD=20`

### Linking Errors with CGAL
```bash
# Force CGAL to use exact arithmetic
cmake .. -DCGAL_DO_NOT_WARN_ABOUT_CMAKE_BUILD_TYPE=TRUE
```

### Memory Issues During Build
```bash
# Reduce parallel jobs
make -j2  # Instead of -j$(nproc)

# Or build specific targets
make TopoCore
make topo-gen
```

### Missing Headers
```bash
# Ensure all development packages are installed
# On Ubuntu: apt-get install *-dev packages
# On CentOS/RHEL: yum install *-devel packages
```

## Success Verification

After successful build, you should see:
- Executable: `build/topo-gen`
- Libraries: `build/src/core/libTopoCore.a`, etc.
- Tests pass: `ctest` shows all green
- Help works: `./topo-gen --help` shows usage

## Distribution Packaging

After building successfully, you can create distribution packages for deployment.

### Quick Packaging

```bash
# Create source package (requires Git)
./scripts/deploy.sh --source

# Create binary package with bundled dependencies
./scripts/deploy.sh --binary --bundle-deps

# Create versioned release
./scripts/create_version.sh "release_description"
```

### Package Outputs

Packages are created in the `dist/` directory:
- **Source**: `dist/source/topo-gen-{version}-source-{timestamp}.tar.gz`
- **Binary**: `dist/{platform}-{arch}/topo-gen-{version}-{platform}-{arch}-{timestamp}.tar.gz`

### Comprehensive Documentation

See `PACKAGING.md` in the project root for complete documentation on:
- Git setup and workflow
- Platform-specific dependency bundling
- Version management
- Package manifest system
- Distribution to package managers (future)

## Next Steps

1. **Test with sample data**: Try the examples in the README
2. **Performance comparison**: Benchmark against Python version
3. **Custom workflows**: Integrate into your GIS pipeline
4. **Create packages**: Use `./scripts/deploy.sh` for distribution
5. **Contribute**: Report issues and submit improvements

---

*For additional help, consult the main README.md, PACKAGING.md, or create an issue on GitHub.*