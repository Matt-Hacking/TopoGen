#!/bin/bash

# macOS Build Script for C++ Topographic Generator
# Optimized for Apple Silicon (M1/M2) with Homebrew

set -e

echo "🍎 Building C++ Topographic Generator on macOS..."
echo "=================================================="

# Set up environment for Apple Silicon Homebrew
export PATH="/opt/homebrew/bin:/opt/homebrew/sbin:$PATH"
export CMAKE_PREFIX_PATH="/opt/homebrew:$CMAKE_PREFIX_PATH"
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
export HOMEBREW_PREFIX="/opt/homebrew"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo "📍 Environment setup:"
echo "PATH: $PATH"
echo "CMAKE_PREFIX_PATH: $CMAKE_PREFIX_PATH"
echo "Homebrew prefix: $HOMEBREW_PREFIX"
echo ""

# Check if Homebrew is available
if ! command -v brew &> /dev/null; then
    echo -e "❌ ${RED}Homebrew not found${NC}"
    echo "Please install Homebrew first: https://brew.sh"
    exit 1
fi

echo -e "✅ ${GREEN}Homebrew found${NC}: $(brew --version | head -1)"
echo ""

# Function to install package if missing
install_if_missing() {
    if brew list "$1" &>/dev/null; then
        echo -e "✅ ${GREEN}$1 already installed${NC}"
    else
        echo -e "📦 ${YELLOW}Installing $1...${NC}"
        brew install "$1"
    fi
}

echo "📦 Checking and installing dependencies..."
echo "-----------------------------------------"

# Install required dependencies
install_if_missing cmake
install_if_missing llvm
install_if_missing cgal
install_if_missing eigen
install_if_missing gdal
install_if_missing tbb
install_if_missing libomp
install_if_missing boost
install_if_missing gmp
install_if_missing mpfr

# Verify libomp installation specifically
if ! brew list libomp &>/dev/null; then
    echo -e "⚠️  ${YELLOW}libomp not found, installing...${NC}"
    brew install libomp
fi

# Set up LLVM/Clang environment for proper OpenMP support
echo "Setting up LLVM/Clang build environment..."
export LLVM=$(brew --prefix llvm)
export PATH="${LLVM}/bin:$PATH"
export COMPILER=${LLVM}/bin/clang++
export CFLAGS="-I/usr/local/include -I${LLVM}/include"
export CXXFLAGS="-I/usr/local/include -I${LLVM}/include"
export LDFLAGS="-L/usr/local/lib -L${LLVM}/lib"
export CXX=${COMPILER}
export CC=${LLVM}/bin/clang

echo "LLVM Environment configured:"
echo "  LLVM: $LLVM"
echo "  PATH: $PATH"
echo "  CXX: $CXX"
echo "  CC: $CC"
echo "  CXXFLAGS: $CXXFLAGS"
echo "  LDFLAGS: $LDFLAGS"

# Auto-detect OpenMP installation (version-agnostic)
OPENMP_DETECTED=false

# Method 1: Try to find any libomp version in Homebrew Cellar
LIBOMP_PATH=$(find /opt/homebrew/Cellar/libomp -name "include" -type d 2>/dev/null | head -1)
if [ -n "$LIBOMP_PATH" ]; then
    LIBOMP_BASE=$(dirname "$LIBOMP_PATH")
    LIBOMP_VERSION=$(basename "$LIBOMP_BASE")
    export OpenMP_ROOT="$LIBOMP_BASE"
    export OpenMP_CXX_FLAGS="-Xpreprocessor -fopenmp -I${LIBOMP_PATH}"
    export OpenMP_CXX_LIB_NAMES="omp"
    export OpenMP_omp_LIBRARY="${LIBOMP_BASE}/lib/libomp.dylib"
    
    # Add OpenMP paths to compiler flags
    export CXXFLAGS="$CXXFLAGS -I${LIBOMP_PATH}"
    export LDFLAGS="$LDFLAGS -L${LIBOMP_BASE}/lib"
    
    echo "OpenMP configured with libomp version: $LIBOMP_VERSION"
    echo "  Include: $LIBOMP_PATH"
    echo "  Library: ${LIBOMP_BASE}/lib/libomp.dylib"
    OPENMP_DETECTED=true
fi

# Method 2: Fallback to system/Homebrew standard paths if Cellar detection failed
if [ "$OPENMP_DETECTED" = false ] && [ -f "/opt/homebrew/include/omp.h" ]; then
    export OpenMP_ROOT="/opt/homebrew"
    export OpenMP_CXX_FLAGS="-Xpreprocessor -fopenmp -I/opt/homebrew/include"
    export OpenMP_CXX_LIB_NAMES="omp"
    export OpenMP_omp_LIBRARY="/opt/homebrew/lib/libomp.dylib"
    
    # Add OpenMP paths to compiler flags
    export CXXFLAGS="$CXXFLAGS -I/opt/homebrew/include"
    export LDFLAGS="$LDFLAGS -L/opt/homebrew/lib"
    
    echo "OpenMP configured with Homebrew standard paths"
    OPENMP_DETECTED=true
fi

if [ "$OPENMP_DETECTED" = false ]; then
    echo "⚠️ OpenMP not detected - parallel processing may not be available"
    echo "   Try: brew install libomp"
fi

echo ""
echo "🔧 Verifying installation..."
echo "----------------------------"

# Verify installations
echo "CMAKE: $(cmake --version | head -1)"
echo "CGAL: $(find /opt/homebrew -name "CGALConfig.cmake" 2>/dev/null | head -1 || echo "Not found in expected location")"
echo "Eigen: $(find /opt/homebrew -name "Eigen3Config.cmake" 2>/dev/null | head -1 || echo "Not found")"
echo "GDAL: $(gdal-config --version 2>/dev/null || echo "Not found")"
echo "TBB: $(find /opt/homebrew -name "*tbb*" -type f 2>/dev/null | head -1 || echo "Not found")"

echo ""

# Set specific paths for other macOS libraries (OpenMP already configured above)
export CGAL_DIR="/opt/homebrew/lib/cmake/CGAL"
export TBB_ROOT="/opt/homebrew"

echo "Library paths configured:"
echo "  CGAL_DIR: $CGAL_DIR"
echo "  TBB_ROOT: $TBB_ROOT"
if [ "$OPENMP_DETECTED" = true ]; then
    echo "  OpenMP_ROOT: $OpenMP_ROOT"
    echo "  OpenMP_omp_LIBRARY: $OpenMP_omp_LIBRARY"
fi

echo "🏗️ Building project..."
echo "----------------------"

# Create build directory
cd "$(dirname "$0")"
rm -rf build
mkdir -p build
cd build

echo "Running cmake configuration..."

# Build CMake command with required arguments
CMAKE_ARGS=(
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_OSX_ARCHITECTURES=arm64
    -DCMAKE_PREFIX_PATH="$CMAKE_PREFIX_PATH"
    -DCMAKE_C_COMPILER="$CC"
    -DCMAKE_CXX_COMPILER="$CXX"
    -DCMAKE_CXX_FLAGS="$CXXFLAGS"
    -DCMAKE_EXE_LINKER_FLAGS="$LDFLAGS"
    -DCGAL_DIR="$CGAL_DIR"
    -DTBB_ROOT="$TBB_ROOT"
    -DCMAKE_VERBOSE_MAKEFILE=ON
)

# Add OpenMP arguments only if detected
if [ "$OPENMP_DETECTED" = true ]; then
    CMAKE_ARGS+=(
        -DOpenMP_ROOT="$OpenMP_ROOT"
        -DOpenMP_CXX_FLAGS="$OpenMP_CXX_FLAGS"
        -DOpenMP_CXX_LIB_NAMES="$OpenMP_CXX_LIB_NAMES"
        -DOpenMP_omp_LIBRARY="$OpenMP_omp_LIBRARY"
    )
    echo "Including OpenMP configuration in CMake..."
else
    echo "Skipping OpenMP configuration (not detected)..."
fi

# Configure with macOS-specific settings and LLVM environment
cmake .. "${CMAKE_ARGS[@]}"

if [ $? -eq 0 ]; then
    echo -e "✅ ${GREEN}CMake configuration successful${NC}"
else
    echo -e "❌ ${RED}CMake configuration failed${NC}"
    echo ""
    echo "Troubleshooting tips:"
    echo "1. Run: ./validate_build_environment.sh"
    echo "2. Check: brew list cgal eigen gdal tbb"
    echo "3. Try: cmake .. -DUSE_FETCHCONTENT_CGAL=ON"
    exit 1
fi

echo ""
echo "Compiling (this may take a few minutes)..."

# Build with parallel jobs
NPROC=$(sysctl -n hw.ncpu)
make -j"$NPROC"

if [ $? -eq 0 ]; then
    echo -e "🎉 ${GREEN}Build successful!${NC}"
    echo ""
    echo "Executable created: $(pwd)/topo-gen"
    
    # Test the executable
    echo "Testing executable..."
    if ./topo-gen --help > /dev/null; then
        echo -e "✅ ${GREEN}Executable test passed${NC}"
        
        # Show binary info
        echo ""
        echo "Binary information:"
        file topo-gen
        echo "Size: $(du -h topo-gen | cut -f1)"
        
        # Check dependencies
        echo ""
        echo "Dynamic library dependencies:"
        otool -L topo-gen | grep -E "(cgal|eigen|gdal|tbb|omp)" || echo "No obvious external dependencies found"
        
    else
        echo -e "⚠️  ${YELLOW}Executable created but test failed${NC}"
    fi
    
    echo ""
    echo "🚀 Ready to use! Try:"
    echo "    ./topo-gen --help"
    echo "    ./topo-gen --bounds \"47.6,-122.3,47.7,-122.2\" --layers 5 --formats stl"
    
else
    echo -e "❌ ${RED}Build failed${NC}"
    echo ""
    echo "Common fixes:"
    echo "1. Clean build: rm -rf * && cmake .. && make"
    echo "2. Update Xcode: xcode-select --install"
    echo "3. Try with fewer cores: make -j2"
    echo "4. Check compiler: which clang++"
    exit 1
fi
