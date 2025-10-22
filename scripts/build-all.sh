#!/bin/bash
#
# build-all.sh - Master build and package script for all architectures
#
# Builds executables and creates distribution packages for all supported
# architectures on the current platform. Designed for both local use and CI/CD.
#
# Copyright (c) 2025 Matthew Block
# Licensed under the MIT License

set -e

# Colors
RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Detect platform and architecture
detect_platform() {
    case "$(uname -s)" in
        Darwin*) echo "macos" ;;
        Linux*)  echo "linux" ;;
        MINGW*|MSYS*|CYGWIN*) echo "windows" ;;
        *) log_error "Unknown platform: $(uname -s)"; exit 1 ;;
    esac
}

detect_architecture() {
    case "$(uname -m)" in
        x86_64|amd64) echo "x86_64" ;;
        aarch64|arm64) echo "arm64" ;;
        *) echo "$(uname -m)" ;;
    esac
}

PLATFORM=$(detect_platform)
ARCHITECTURE=$(detect_architecture)
BUILD_TYPE="${BUILD_TYPE:-Release}"

log_info "=================================================="
log_info "  Multi-Architecture Build & Package System"
log_info "=================================================="
log_info "Platform: $PLATFORM"
log_info "Architecture: $ARCHITECTURE"
log_info "Build Type: $BUILD_TYPE"
echo ""

# Parse arguments
NATIVE_ONLY=false
CROSS_COMPILE=false
CLEAN_BUILD=false
SKIP_PACKAGES=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --native-only)
            NATIVE_ONLY=true
            shift
            ;;
        --cross-compile)
            CROSS_COMPILE=true
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --skip-packages)
            SKIP_PACKAGES=true
            shift
            ;;
        --help)
            cat << EOF
Usage: $0 [OPTIONS]

Master script to build and package for all architectures.

Options:
    --native-only      Build only for native architecture (default)
    --cross-compile    Attempt cross-compilation if toolchains available
    --clean            Clean build directories before building
    --skip-packages    Build only, don't create packages
    --help             Show this help message

Examples:
    # Build native architecture and create packages
    $0

    # Clean build with packages
    $0 --clean

    # Build only, no packages
    $0 --skip-packages

    # Attempt cross-compilation (experimental)
    $0 --cross-compile

Platform-specific behavior:
    macOS:   Builds for current arch, creates DMG/PKG/Homebrew
    Windows: Builds for current arch, creates MSI/WinGet/Portable ZIP
    Linux:   Builds for current arch, creates DEB/RPM/Flatpak/AppImage

CI/CD Usage:
    This script is called by GitHub Actions for each platform/architecture.
    See .github/workflows/build-packages.yml for the complete workflow.

EOF
            exit 0
            ;;
        *)
            log_error "Unknown argument: $1"
            exit 1
            ;;
    esac
done

cd "$PROJECT_ROOT"

# Clean if requested
if [[ "$CLEAN_BUILD" == true ]]; then
    log_info "Cleaning build directories..."
    rm -rf build dist
    log_success "Cleaned"
fi

# Build native architecture
log_info "Building for native architecture ($ARCHITECTURE)..."

cmake -B build \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DCMAKE_INSTALL_PREFIX="$PROJECT_ROOT/install"

cmake --build build --config $BUILD_TYPE --parallel

log_success "Build complete"

# Test the build
log_info "Testing build..."
if [[ "$PLATFORM" == "macos" || "$PLATFORM" == "linux" ]]; then
    ./build/topo-gen --version || log_warning "CLI test failed"
elif [[ "$PLATFORM" == "windows" ]]; then
    ./build/Release/topo-gen.exe --version || log_warning "CLI test failed"
fi

# Bundle dependencies
log_info "Bundling dependencies..."

case "$PLATFORM" in
    macos)
        if [[ -f "packaging/macos/bundle_deps.sh" ]]; then
            ./packaging/macos/bundle_deps.sh \
                --executable build/topo-gen \
                --output dist/macos-deps
            log_success "Dependencies bundled"
        fi
        ;;
    windows)
        if [[ -f "packaging/windows/bundle_deps.sh" ]]; then
            ./packaging/windows/bundle_deps.sh \
                --executable build/Release/topo-gen.exe \
                --output dist/windows-deps
            log_success "Dependencies bundled"
        fi
        ;;
    linux)
        if [[ -f "packaging/linux/bundle_deps.sh" ]]; then
            ./packaging/linux/bundle_deps.sh \
                --executable build/topo-gen \
                --output dist/linux-deps
            log_success "Dependencies bundled"
        fi
        ;;
esac

# Create packages unless skipped
if [[ "$SKIP_PACKAGES" == false ]]; then
    log_info "Creating distribution packages..."

    case "$PLATFORM" in
        macos)
            ./scripts/deploy.sh --all-macos
            ;;
        windows)
            ./scripts/deploy.sh --all-windows
            ;;
        linux)
            ./scripts/deploy.sh --all-linux
            ;;
    esac

    log_success "Packages created"

    # Show what was created
    echo ""
    log_info "Created packages:"
    find dist -type f \( \
        -name "*.dmg" -o \
        -name "*.pkg" -o \
        -name "*.deb" -o \
        -name "*.rpm" -o \
        -name "*.msi" -o \
        -name "*.zip" -o \
        -name "*.AppImage" -o \
        -name "*.tar.gz" -o \
        -name "*.rb" \
    \) -exec ls -lh {} \;
fi

# Cross-compilation (experimental)
if [[ "$CROSS_COMPILE" == true && "$NATIVE_ONLY" == false ]]; then
    log_info "Attempting cross-compilation..."

    case "$PLATFORM" in
        macos)
            # Try universal binary
            if [[ -f "cmake/toolchains/macos-universal.cmake" ]]; then
                log_info "Building macOS universal binary..."
                cmake -B build-universal \
                    -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/macos-universal.cmake \
                    -DCMAKE_BUILD_TYPE=$BUILD_TYPE

                cmake --build build-universal --config $BUILD_TYPE --parallel
                log_success "Universal binary built"
            fi
            ;;

        linux)
            # Try ARM64 cross-compile if on x86_64
            if [[ "$ARCHITECTURE" == "x86_64" && -f "cmake/toolchains/linux-aarch64.cmake" ]]; then
                if command -v aarch64-linux-gnu-gcc &> /dev/null; then
                    log_info "Cross-compiling for ARM64..."
                    cmake -B build-arm64 \
                        -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-aarch64.cmake \
                        -DCMAKE_BUILD_TYPE=$BUILD_TYPE

                    cmake --build build-arm64 --config $BUILD_TYPE --parallel
                    log_success "ARM64 cross-compile complete"
                else
                    log_warning "ARM64 cross-compiler not found (install gcc-aarch64-linux-gnu)"
                fi
            fi
            ;;
    esac
fi

echo ""
log_success "=========================================="
log_success "  Build & Package Complete!"
log_success "=========================================="
echo ""

log_info "Next steps:"
case "$PLATFORM" in
    macos)
        echo "  Test DMG: open dist/macos-arm64/*.dmg"
        echo "  Test PKG: open dist/macos-arm64/*.pkg"
        ;;
    windows)
        echo "  Test MSI: dist/windows-x64/wix-sources/build.bat"
        echo "  Test Portable: Extract and run dist/windows-x64/*-portable.zip"
        ;;
    linux)
        echo "  Test DEB: sudo dpkg -i dist/linux-x64/*.deb"
        echo "  Test RPM: sudo rpm -i dist/linux-x64/*.rpm"
        echo "  Test AppImage: chmod +x dist/linux-x64/*.AppImage && ./dist/linux-x64/*.AppImage"
        ;;
esac

echo ""
log_info "For CI/CD usage, see .github/workflows/build-packages.yml"
