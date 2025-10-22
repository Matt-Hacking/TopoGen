#!/bin/bash
# Build script for GUI application

set -e  # Exit on error

# Get project root directory (parent of scripts/)
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "=== Building Topographic Generator GUI ==="
echo "Project root: $PROJECT_ROOT"

# Create build directory
mkdir -p build
cd build

# Configure with CMake (enable GUI, disable testing/benchmarks for faster builds)
echo "Configuring with CMake..."
cmake -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_GUI=ON \
      -DENABLE_TESTING=OFF \
      -BUILD_BENCHMARKS=OFF \
      ..

# Build
echo "Building..."
cmake --build . -j$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)

echo ""
echo "=== Build Complete ==="
echo "CLI executable: build/topo-gen"
echo "GUI executable: build/src/gui/topo-gen-gui"
echo ""
echo "To run GUI: ./build/src/gui/topo-gen-gui"
