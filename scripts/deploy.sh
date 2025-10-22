#!/bin/bash
#
# deploy.sh - C++ Topographic Generator Deployment Script
#
# Git-aware deployment orchestrator that creates platform-specific
# distribution packages with bundled dependencies.
#
# Copyright (c) 2025 Matthew Block
# Licensed under the MIT License
#

set -e  # Exit on any error

# Configuration
PROJECT_NAME="topographic-generator"
BUILD_TYPE="${BUILD_TYPE:-Release}"

# Get script directory and project root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PACKAGING_DIR="$PROJECT_ROOT/packaging"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Function to extract version from CMakeLists.txt
get_version() {
    grep -E "^project\\(.*VERSION [0-9]+\\.[0-9]+\\.[0-9]+" "$PROJECT_ROOT/CMakeLists.txt" | \
        sed -E 's/.*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/'
}

# Function to get git commit hash
get_git_commit() {
    if command -v git &> /dev/null && [[ -d "$PROJECT_ROOT/.git" ]]; then
        git rev-parse --short HEAD 2>/dev/null || echo "unknown"
    else
        echo "nogit"
    fi
}

# Function to get git tag
get_git_tag() {
    if command -v git &> /dev/null && [[ -d "$PROJECT_ROOT/.git" ]]; then
        git describe --tags --exact-match 2>/dev/null || echo "untagged"
    else
        echo "notag"
    fi
}

# Function to detect platform
detect_platform() {
    case "$(uname -s)" in
        Darwin*)
            echo "macos"
            ;;
        Linux*)
            echo "linux"
            ;;
        MINGW*|MSYS*|CYGWIN*)
            echo "windows"
            ;;
        *)
            log_error "Unknown platform: $(uname -s)"
            exit 1
            ;;
    esac
}

# Function to detect architecture
detect_architecture() {
    case "$(uname -m)" in
        x86_64|amd64)
            echo "x64"
            ;;
        aarch64|arm64)
            echo "arm64"
            ;;
        *)
            echo "$(uname -m)"
            ;;
    esac
}

# Function to generate version.json manifest
generate_version_manifest() {
    local output_file="$1"
    local package_format="$2"
    local package_filename="$3"

    local version=$(get_version)
    local git_commit=$(get_git_commit)
    local git_tag=$(get_git_tag)
    local platform=$(detect_platform)
    local arch=$(detect_architecture)
    local build_date=$(date -u +"%Y-%m-%dT%H:%M:%SZ")

    # Get dependency versions (if available)
    local gdal_version="unknown"
    local qt_version="unknown"

    if command -v gdal-config &> /dev/null; then
        gdal_version=$(gdal-config --version)
    fi

    # Calculate checksums if file exists
    local sha256="pending"
    local md5="pending"
    local size="pending"

    if [[ -f "$package_filename" ]]; then
        if command -v shasum &> /dev/null; then
            sha256=$(shasum -a 256 "$package_filename" | awk '{print $1}')
        elif command -v sha256sum &> /dev/null; then
            sha256=$(sha256sum "$package_filename" | awk '{print $1}')
        fi

        if command -v md5 &> /dev/null; then
            md5=$(md5 -q "$package_filename")
        elif command -v md5sum &> /dev/null; then
            md5=$(md5sum "$package_filename" | awk '{print $1}')
        fi

        size=$(stat -f%z "$package_filename" 2>/dev/null || stat -c%s "$package_filename" 2>/dev/null || echo "unknown")
    fi

    # Generate JSON from template
    if [[ -f "$PACKAGING_DIR/common/version.json.template" ]]; then
        sed -e "s/\${VERSION}/$version/g" \
            -e "s/\${BUILD_DATE}/$build_date/g" \
            -e "s/\${GIT_COMMIT}/$git_commit/g" \
            -e "s/\${GIT_TAG}/$git_tag/g" \
            -e "s/\${PLATFORM}/$platform/g" \
            -e "s/\${ARCHITECTURE}/$arch/g" \
            -e "s/\${BUILD_TYPE}/$BUILD_TYPE/g" \
            -e "s/\${GDAL_VERSION}/$gdal_version/g" \
            -e "s/\${CGAL_VERSION}/unknown/g" \
            -e "s/\${EIGEN_VERSION}/unknown/g" \
            -e "s/\${TBB_VERSION}/unknown/g" \
            -e "s/\${QT_VERSION}/$qt_version/g" \
            -e "s/\${OPENMP_VERSION}/unknown/g" \
            -e "s/\${SHA256}/$sha256/g" \
            -e "s/\${MD5}/$md5/g" \
            -e "s/\${PACKAGE_FORMAT}/$package_format/g" \
            -e "s|\${PACKAGE_FILENAME}|$(basename "$package_filename")|g" \
            -e "s/\${PACKAGE_SIZE}/$size/g" \
            "$PACKAGING_DIR/common/version.json.template" > "$output_file"
    else
        log_warning "version.json.template not found, creating basic manifest"
        cat > "$output_file" << EOF
{
  "name": "Topographic Generator",
  "version": "$version",
  "build": {
    "date": "$build_date",
    "commit": "$git_commit",
    "tag": "$git_tag",
    "platform": "$platform",
    "architecture": "$arch"
  }
}
EOF
    fi

    log_success "Generated version manifest: $output_file"
}

# Usage information
usage() {
    cat << EOF
==========================================
C++ Topographic Generator Deployment Script
Version: $(get_version)
==========================================

Usage: $0 [OPTIONS]

Package Format Options:
    --source            Create source tarball (git archive)
    --binary            Create binary package for current platform
    --platform PLATFORM Target platform: macos, linux, windows, source
    --all               Create all packages (source + binary for current platform)

Build Options:
    --clean-build       Clean and rebuild before packaging
    --build-type TYPE   Build type: Debug, Release (default: Release)

Bundling Options:
    --bundle-deps       Bundle dependencies with binary packages
    --no-bundle-deps    Skip dependency bundling (default: bundle)

Output Options:
    --output-dir DIR    Output directory (default: PROJECT_ROOT/dist)

Other Options:
    --help              Show this help message

Examples:
    # Create source tarball using git archive
    $0 --source

    # Create binary package for current platform with dependencies
    $0 --binary --bundle-deps

    # Build everything
    $0 --all --clean-build

    # Create macOS package
    $0 --platform macos --binary --bundle-deps

Environment Variables:
    BUILD_TYPE          Build configuration (Debug/Release, default: Release)

EOF
    exit 0
}

# Default options
CREATE_SOURCE=false
CREATE_BINARY=false
CLEAN_BUILD=false
BUNDLE_DEPS=true
OUTPUT_DIR="$PROJECT_ROOT/dist"
TARGET_PLATFORM=""

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --source)
            CREATE_SOURCE=true
            shift
            ;;
        --binary)
            CREATE_BINARY=true
            shift
            ;;
        --platform)
            TARGET_PLATFORM="$2"
            shift 2
            ;;
        --all)
            CREATE_SOURCE=true
            CREATE_BINARY=true
            shift
            ;;
        --clean-build)
            CLEAN_BUILD=true
            shift
            ;;
        --build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        --bundle-deps)
            BUNDLE_DEPS=true
            shift
            ;;
        --no-bundle-deps)
            BUNDLE_DEPS=false
            shift
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --help)
            usage
            ;;
        *)
            log_error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Default to binary if nothing specified
if [[ "$CREATE_SOURCE" == false && "$CREATE_BINARY" == false ]]; then
    CREATE_BINARY=true
fi

# Detect platform if not specified
if [[ -z "$TARGET_PLATFORM" ]]; then
    TARGET_PLATFORM=$(detect_platform)
fi

ARCHITECTURE=$(detect_architecture)
VERSION=$(get_version)

log_info "Deployment Configuration"
log_info "  Version: $VERSION"
log_info "  Platform: $TARGET_PLATFORM"
log_info "  Architecture: $ARCHITECTURE"
log_info "  Build Type: $BUILD_TYPE"
log_info "  Source package: $CREATE_SOURCE"
log_info "  Binary package: $CREATE_BINARY"
log_info "  Bundle dependencies: $BUNDLE_DEPS"
log_info "  Output directory: $OUTPUT_DIR"
echo ""

# Change to project root
cd "$PROJECT_ROOT"

# Create output directory structure
mkdir -p "$OUTPUT_DIR/source"
mkdir -p "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"

# Clean build if requested
if [[ "$CLEAN_BUILD" == true ]]; then
    log_info "Cleaning previous build..."
    if [[ -d build ]]; then
        rm -rf build
    fi
fi

# Build if creating binary package
if [[ "$CREATE_BINARY" == true ]]; then
    if [[ ! -f build/topo-gen ]] || [[ "$CLEAN_BUILD" == true ]]; then
        log_info "Building project..."
        mkdir -p build
        cd build
        cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
        cd ..
        log_success "Build completed"
    else
        log_info "Build artifacts already exist, skipping build"
    fi

    # Verify executables
    if [[ ! -f build/topo-gen ]]; then
        log_error "CLI executable not found: build/topo-gen"
        exit 1
    fi
    log_success "CLI executable verified: build/topo-gen"

    if [[ -f build/topo-gen-gui || -d build/topo-gen-gui.app ]]; then
        log_success "GUI executable verified"
    fi
fi

# Create source package
if [[ "$CREATE_SOURCE" == true ]]; then
    log_info "Creating source package..."

    TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
    SOURCE_TARBALL="topo-gen-${VERSION}-source-${TIMESTAMP}.tar.gz"
    SOURCE_PATH="$OUTPUT_DIR/source/$SOURCE_TARBALL"

    # Check if git is available
    if command -v git &> /dev/null && [[ -d ".git" ]]; then
        log_info "Using git archive for source package..."
        git archive --format=tar.gz --prefix="topo-gen-${VERSION}/" HEAD > "$SOURCE_PATH"
        log_success "Source tarball created with git archive"
    else
        log_warning "Git not available, using manual tar creation..."
        # Fallback to manual tar creation
        TEMP_DIR=$(mktemp -d)
        STAGING_DIR="$TEMP_DIR/topo-gen-${VERSION}"
        mkdir -p "$STAGING_DIR"

        # Copy essential files (similar to old deploy.sh)
        cp -r src include external scripts docs CMakeLists.txt LICENSE COPYRIGHT README.md "$STAGING_DIR/"

        # Create tarball
        cd "$TEMP_DIR"
        tar -czf "$SOURCE_PATH" "topo-gen-${VERSION}"
        cd "$PROJECT_ROOT"

        # Cleanup
        rm -rf "$TEMP_DIR"

        log_success "Source tarball created manually"
    fi

    # Generate version manifest
    generate_version_manifest "$OUTPUT_DIR/source/version.json" "source-tarball" "$SOURCE_PATH"

    # Show result
    SIZE=$(du -h "$SOURCE_PATH" | cut -f1)
    log_success "Source package: $SOURCE_TARBALL ($SIZE)"
fi

# Create binary package
if [[ "$CREATE_BINARY" == true ]]; then
    log_info "Creating binary package for $TARGET_PLATFORM-$ARCHITECTURE..."

    TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
    BINARY_DIR="$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"
    PACKAGE_NAME="topo-gen-${VERSION}-${TARGET_PLATFORM}-${ARCHITECTURE}-${TIMESTAMP}"

    # Create staging directory
    STAGING_DIR="$BINARY_DIR/$PACKAGE_NAME"
    mkdir -p "$STAGING_DIR/bin"
    mkdir -p "$STAGING_DIR/lib"
    mkdir -p "$STAGING_DIR/share"

    # Copy executables
    cp build/topo-gen "$STAGING_DIR/bin/"
    log_success "Copied CLI executable"

    if [[ "$TARGET_PLATFORM" == "macos" ]]; then
        if [[ -d build/topo-gen-gui.app ]]; then
            cp -R build/topo-gen-gui.app "$STAGING_DIR/"
            log_success "Copied GUI application bundle"
        fi
    elif [[ -f build/topo-gen-gui ]]; then
        cp build/topo-gen-gui "$STAGING_DIR/bin/"
        log_success "Copied GUI executable"
    fi

    # Bundle dependencies if requested
    if [[ "$BUNDLE_DEPS" == true ]]; then
        log_info "Bundling dependencies..."

        BUNDLER_SCRIPT="$PACKAGING_DIR/${TARGET_PLATFORM}/bundle_deps.sh"

        if [[ -f "$BUNDLER_SCRIPT" ]]; then
            # Bundle CLI dependencies
            "$BUNDLER_SCRIPT" --cli "$STAGING_DIR/bin/topo-gen" --output-dir "$STAGING_DIR"

            # Bundle GUI dependencies if present
            if [[ "$TARGET_PLATFORM" == "macos" && -d "$STAGING_DIR/topo-gen-gui.app" ]]; then
                "$BUNDLER_SCRIPT" --gui "$STAGING_DIR/topo-gen-gui.app"
            elif [[ -f "$STAGING_DIR/bin/topo-gen-gui" ]]; then
                "$BUNDLER_SCRIPT" --gui "$STAGING_DIR/bin/topo-gen-gui" --output-dir "$STAGING_DIR"
            fi

            log_success "Dependencies bundled"
        else
            log_warning "Bundler script not found: $BUNDLER_SCRIPT"
            log_warning "Dependencies not bundled"
        fi
    fi

    # Copy documentation
    cp LICENSE "$STAGING_DIR/"
    cp COPYRIGHT "$STAGING_DIR/"
    cp README.md "$STAGING_DIR/"

    # Create package based on platform
    case "$TARGET_PLATFORM" in
        macos)
            PACKAGE_FILE="$BINARY_DIR/${PACKAGE_NAME}.tar.gz"
            cd "$BINARY_DIR"
            tar -czf "$(basename "$PACKAGE_FILE")" "$PACKAGE_NAME"
            cd "$PROJECT_ROOT"
            ;;
        windows)
            PACKAGE_FILE="$BINARY_DIR/${PACKAGE_NAME}.zip"
            cd "$BINARY_DIR"
            zip -r "$(basename "$PACKAGE_FILE")" "$PACKAGE_NAME" >/dev/null
            cd "$PROJECT_ROOT"
            ;;
        linux)
            PACKAGE_FILE="$BINARY_DIR/${PACKAGE_NAME}.tar.gz"
            cd "$BINARY_DIR"
            tar -czf "$(basename "$PACKAGE_FILE")" "$PACKAGE_NAME"
            cd "$PROJECT_ROOT"
            ;;
    esac

    # Remove staging directory
    rm -rf "$STAGING_DIR"

    # Generate version manifest
    generate_version_manifest "$BINARY_DIR/version.json" "binary-${TARGET_PLATFORM}" "$PACKAGE_FILE"

    # Show result
    SIZE=$(du -h "$PACKAGE_FILE" | cut -f1)
    log_success "Binary package: $(basename "$PACKAGE_FILE") ($SIZE)"
fi

echo ""
log_success "Deployment completed successfully!"
log_info "Output directory: $OUTPUT_DIR"
echo ""

# Show what was created
if [[ "$CREATE_SOURCE" == true ]]; then
    echo "Source package:"
    ls -lh "$OUTPUT_DIR/source"/*.tar.gz 2>/dev/null | tail -1 | awk '{print "  " $9, "(" $5 ")"}'
fi

if [[ "$CREATE_BINARY" == true ]]; then
    echo "Binary package:"
    ls -lh "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"/*.{tar.gz,zip} 2>/dev/null | tail -1 | awk '{print "  " $9, "(" $5 ")"}'
fi

echo ""
log_info "To extract and test:"
if [[ "$TARGET_PLATFORM" == "windows" ]]; then
    echo "  unzip topo-gen-*.zip"
else
    echo "  tar -xzf topo-gen-*.tar.gz"
fi
echo "  cd topo-gen-*/"
echo "  ./bin/topo-gen --help"
echo ""
