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
    --dmg               Create macOS DMG installer (macOS only)
    --pkg               Create macOS PKG installer (macOS only)
    --homebrew          Generate Homebrew formula
    --msi               Create Windows MSI installer (generates WiX sources)
    --winget            Generate WinGet package manifest
    --portable-zip      Create portable Windows ZIP package
    --deb               Create Debian/Ubuntu DEB package
    --rpm               Create Fedora/RHEL RPM package
    --source-deb        Create Debian source package (.dsc, .debian.tar.xz, .orig.tar.gz)
    --source-rpm        Create RPM source package (.src.rpm)
    --flatpak           Generate Flatpak manifest
    --appimage          Create AppImage portable package
    --platform PLATFORM Target platform: macos, linux, windows, source
    --all               Create all packages (source + binary for current platform)
    --all-macos         Create all macOS packages (binary, DMG, PKG, Homebrew)
    --all-windows       Create all Windows packages (binary, MSI, WinGet, portable ZIP)
    --all-linux         Create all Linux packages (binary, DEB, RPM, Flatpak, AppImage)

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

    # Create macOS DMG installer
    $0 --dmg

    # Create macOS PKG installer
    $0 --pkg

    # Generate Homebrew formula
    $0 --homebrew

    # Create all macOS packages
    $0 --all-macos --clean-build

    # Create Windows MSI installer (WiX sources)
    $0 --msi

    # Generate WinGet manifest
    $0 --winget

    # Create portable Windows ZIP
    $0 --portable-zip

    # Create all Windows packages
    $0 --all-windows --clean-build

    # Create specific platform package
    $0 --platform macos --binary --bundle-deps

Environment Variables:
    BUILD_TYPE          Build configuration (Debug/Release, default: Release)

EOF
    exit 0
}

# Default options
CREATE_SOURCE=false
CREATE_BINARY=false
CREATE_DMG=false
CREATE_PKG=false
CREATE_HOMEBREW=false
CREATE_MSI=false
CREATE_WINGET=false
CREATE_PORTABLE_ZIP=false
CREATE_DEB=false
CREATE_RPM=false
CREATE_SOURCE_DEB=false
CREATE_SOURCE_RPM=false
CREATE_FLATPAK=false
CREATE_APPIMAGE=false
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
        --dmg)
            CREATE_DMG=true
            shift
            ;;
        --pkg)
            CREATE_PKG=true
            shift
            ;;
        --homebrew)
            CREATE_HOMEBREW=true
            shift
            ;;
        --msi)
            CREATE_MSI=true
            shift
            ;;
        --winget)
            CREATE_WINGET=true
            shift
            ;;
        --portable-zip)
            CREATE_PORTABLE_ZIP=true
            shift
            ;;
        --deb)
            CREATE_DEB=true
            shift
            ;;
        --rpm)
            CREATE_RPM=true
            shift
            ;;
        --source-deb)
            CREATE_SOURCE_DEB=true
            shift
            ;;
        --source-rpm)
            CREATE_SOURCE_RPM=true
            shift
            ;;
        --flatpak)
            CREATE_FLATPAK=true
            shift
            ;;
        --appimage)
            CREATE_APPIMAGE=true
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
        --all-macos)
            CREATE_SOURCE=true
            CREATE_BINARY=true
            CREATE_DMG=true
            CREATE_PKG=true
            CREATE_HOMEBREW=true
            shift
            ;;
        --all-windows)
            CREATE_SOURCE=true
            CREATE_BINARY=true
            CREATE_MSI=true
            CREATE_WINGET=true
            CREATE_PORTABLE_ZIP=true
            shift
            ;;
        --all-linux)
            CREATE_SOURCE=true
            CREATE_BINARY=true
            CREATE_DEB=true
            CREATE_RPM=true
            CREATE_SOURCE_DEB=true
            CREATE_SOURCE_RPM=true
            CREATE_FLATPAK=true
            CREATE_APPIMAGE=true
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
    # Check if build artifacts exist (platform-specific paths)
    BUILD_EXISTS=false
    if [[ "$TARGET_PLATFORM" == "windows" ]]; then
        [[ -f build/Release/topo-gen.exe ]] && BUILD_EXISTS=true
    else
        [[ -f build/topo-gen ]] && BUILD_EXISTS=true
    fi

    if [[ "$BUILD_EXISTS" == false ]] || [[ "$CLEAN_BUILD" == true ]]; then
        log_info "Building project..."
        mkdir -p build
        cd build
        cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

        # Use cmake --build for cross-platform compatibility
        if [[ "$TARGET_PLATFORM" == "windows" ]]; then
            cmake --build . --config "$BUILD_TYPE" --parallel
        else
            make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
        fi
        cd ..
        log_success "Build completed"
    else
        log_info "Build artifacts already exist, skipping build"
    fi

    # Verify executables (platform-specific paths)
    if [[ "$TARGET_PLATFORM" == "windows" ]]; then
        if [[ ! -f build/Release/topo-gen.exe ]]; then
            log_error "CLI executable not found: build/Release/topo-gen.exe"
            exit 1
        fi
        log_success "CLI executable verified: build/Release/topo-gen.exe"

        if [[ -f build/Release/topo-gen-gui.exe ]]; then
            log_success "GUI executable verified: build/Release/topo-gen-gui.exe"
        fi
    else
        if [[ ! -f build/topo-gen ]]; then
            log_error "CLI executable not found: build/topo-gen"
            exit 1
        fi
        log_success "CLI executable verified: build/topo-gen"

        if [[ -f build/topo-gen-gui || -d build/topo-gen-gui.app ]]; then
            log_success "GUI executable verified"
        fi
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

    # Copy executables (platform-specific paths)
    if [[ "$TARGET_PLATFORM" == "windows" ]]; then
        cp build/Release/topo-gen.exe "$STAGING_DIR/bin/"
        log_success "Copied CLI executable"

        if [[ -f build/Release/topo-gen-gui.exe ]]; then
            cp build/Release/topo-gen-gui.exe "$STAGING_DIR/bin/"
            log_success "Copied GUI executable"
        fi
    else
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
    fi

    # Bundle dependencies if requested (skip on Windows - handled by MSI/ZIP packaging)
    if [[ "$BUNDLE_DEPS" == true && "$TARGET_PLATFORM" != "windows" ]]; then
        log_info "Bundling dependencies..."

        BUNDLER_SCRIPT="$PACKAGING_DIR/${TARGET_PLATFORM}/bundle_deps.sh"

        if [[ -f "$BUNDLER_SCRIPT" ]]; then
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
    elif [[ "$TARGET_PLATFORM" == "windows" ]]; then
        log_info "Skipping dependency bundling (Windows uses MSI/ZIP packaging)"
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

# Create DMG (macOS only)
if [[ "$CREATE_DMG" == true ]]; then
    if [[ "$TARGET_PLATFORM" != "macos" ]]; then
        log_warning "DMG creation only supported on macOS, skipping"
    elif [[ ! -d "build/topo-gen-gui.app" ]]; then
        log_warning "GUI app bundle not found, skipping DMG creation"
        log_info "Build GUI first: cmake --build build --target topo-gen-gui"
    else
        log_info "Creating DMG installer..."

        DMG_SCRIPT="$SCRIPT_DIR/package/create-dmg.sh"
        if [[ -x "$DMG_SCRIPT" ]]; then
            "$DMG_SCRIPT" \
                --output-dir "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}" \
                build/topo-gen-gui.app
            log_success "DMG created"
        else
            log_error "DMG script not found or not executable: $DMG_SCRIPT"
        fi
    fi
fi

# Create PKG (macOS only)
if [[ "$CREATE_PKG" == true ]]; then
    if [[ "$TARGET_PLATFORM" != "macos" ]]; then
        log_warning "PKG creation only supported on macOS, skipping"
    elif [[ ! -f "build/topo-gen" ]]; then
        log_warning "CLI executable not found, skipping PKG creation"
        log_info "Build first: cmake --build build"
    else
        log_info "Creating PKG installer..."

        PKG_SCRIPT="$SCRIPT_DIR/package/create-pkg.sh"
        if [[ -x "$PKG_SCRIPT" ]]; then
            PKG_CMD=("$PKG_SCRIPT" \
                --cli build/topo-gen \
                --output-dir "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}" \
                --version "$VERSION")

            # Add GUI if available
            if [[ -d "build/topo-gen-gui.app" ]]; then
                PKG_CMD+=(--gui build/topo-gen-gui.app)
            else
                PKG_CMD+=(--no-gui)
            fi

            "${PKG_CMD[@]}"
            log_success "PKG created"
        else
            log_error "PKG script not found or not executable: $PKG_SCRIPT"
        fi
    fi
fi

# Create Homebrew formula
if [[ "$CREATE_HOMEBREW" == true ]]; then
    log_info "Generating Homebrew formula..."

    HOMEBREW_SCRIPT="$SCRIPT_DIR/package/create-homebrew.sh"

    # Check if source tarball exists
    SOURCE_TARBALL=$(ls -t "$OUTPUT_DIR/source"/topo-gen-${VERSION}-source-*.tar.gz 2>/dev/null | head -1)

    if [[ -z "$SOURCE_TARBALL" ]]; then
        log_warning "Source tarball not found, creating one first..."
        # Create source package if not already done
        TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
        SOURCE_TARBALL_NAME="topo-gen-${VERSION}-source-${TIMESTAMP}.tar.gz"
        SOURCE_TARBALL="$OUTPUT_DIR/source/$SOURCE_TARBALL_NAME"

        mkdir -p "$OUTPUT_DIR/source"

        if command -v git &> /dev/null && [[ -d ".git" ]]; then
            git archive --format=tar.gz --prefix="topo-gen-${VERSION}/" HEAD > "$SOURCE_TARBALL"
            log_success "Created source tarball for Homebrew"
        else
            log_error "Git not available, cannot create source tarball"
            log_info "Create source tarball first: $0 --source"
            CREATE_HOMEBREW=false
        fi
    fi

    if [[ "$CREATE_HOMEBREW" == true && -f "$SOURCE_TARBALL" ]]; then
        # For Homebrew, we need a public URL
        # Generate formula with placeholder URL that user must update
        TARBALL_URL="https://github.com/matthewblock/topo-gen/archive/v${VERSION}.tar.gz"

        if [[ -x "$HOMEBREW_SCRIPT" ]]; then
            "$HOMEBREW_SCRIPT" \
                --tarball "$TARBALL_URL" \
                --version "$VERSION" \
                --output-dir "$OUTPUT_DIR/homebrew"

            log_success "Homebrew formula created"
            log_warning "Update the URL in the formula to point to your actual release"
        else
            log_error "Homebrew script not found or not executable: $HOMEBREW_SCRIPT"
        fi
    fi
fi

# Create MSI installer (Windows only)
if [[ "$CREATE_MSI" == true ]]; then
    log_info "Creating MSI installer (WiX sources)..."

    MSI_SCRIPT="$SCRIPT_DIR/package/create-msi.sh"

    # Check if dependencies are bundled
    DEPS_DIR="$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}/topo-gen-${VERSION}/lib"

    if [[ ! -d "$DEPS_DIR" ]]; then
        # Try alternative location
        DEPS_DIR="dist/windows-deps"
        if [[ ! -d "$DEPS_DIR" ]]; then
            log_warning "Dependencies directory not found"
            log_info "Run with --binary --bundle-deps first, or specify --deps-dir"
            CREATE_MSI=false
        fi
    fi

    if [[ "$CREATE_MSI" == true && -x "$MSI_SCRIPT" ]]; then
        MSI_CMD=(
            "$MSI_SCRIPT"
            --version "$VERSION"
            --output-dir "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"
        )

        # Add CLI if exists
        if [[ -f "build/topo-gen.exe" ]]; then
            MSI_CMD+=(--cli "build/topo-gen.exe")
        else
            MSI_CMD+=(--no-cli)
        fi

        # Add GUI if exists
        if [[ -f "build/topo-gen-gui.exe" ]]; then
            MSI_CMD+=(--gui "build/topo-gen-gui.exe")
        else
            MSI_CMD+=(--no-gui)
        fi

        # Add deps directory
        MSI_CMD+=(--deps-dir "$DEPS_DIR")

        "${MSI_CMD[@]}"
        log_success "MSI WiX sources created"
        log_info "To compile on Windows: cd wix-sources && build.bat"
    elif [[ ! -x "$MSI_SCRIPT" ]]; then
        log_error "MSI script not found or not executable: $MSI_SCRIPT"
    fi
fi

# Create WinGet manifest
if [[ "$CREATE_WINGET" == true ]]; then
    log_info "Generating WinGet manifest..."

    WINGET_SCRIPT="$SCRIPT_DIR/package/create-winget.sh"

    # Check if MSI exists or use placeholder
    MSI_FILE=$(ls -t "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"/*.msi 2>/dev/null | head -1)

    if [[ -z "$MSI_FILE" ]]; then
        log_warning "MSI file not found, using placeholder URL"
        MSI_URL="https://github.com/matthewblock/topo-gen/releases/download/v${VERSION}/topo-gen-${VERSION}.msi"
    else
        MSI_URL="$MSI_FILE"
    fi

    if [[ -x "$WINGET_SCRIPT" ]]; then
        "$WINGET_SCRIPT" \
            --installer-url "$MSI_URL" \
            --version "$VERSION" \
            --output-dir "$OUTPUT_DIR/winget"

        log_success "WinGet manifest created"
        if [[ -z "$MSI_FILE" ]]; then
            log_warning "Update installer URL in manifest before submission"
        fi
    else
        log_error "WinGet script not found or not executable: $WINGET_SCRIPT"
    fi
fi

# Create portable ZIP (Windows)
if [[ "$CREATE_PORTABLE_ZIP" == true ]]; then
    log_info "Creating portable Windows ZIP package..."

    PORTABLE_SCRIPT="$SCRIPT_DIR/package/create-portable-zip.sh"

    # Check if dependencies are bundled
    DEPS_DIR="$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}/topo-gen-${VERSION}/lib"

    if [[ ! -d "$DEPS_DIR" ]]; then
        DEPS_DIR="dist/windows-deps"
        if [[ ! -d "$DEPS_DIR" ]]; then
            log_warning "Dependencies directory not found"
            log_info "Run with --binary --bundle-deps first"
            CREATE_PORTABLE_ZIP=false
        fi
    fi

    if [[ "$CREATE_PORTABLE_ZIP" == true && -x "$PORTABLE_SCRIPT" ]]; then
        PORTABLE_CMD=(
            "$PORTABLE_SCRIPT"
            --version "$VERSION"
            --output-dir "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"
            --deps-dir "$DEPS_DIR"
        )

        # Add CLI if exists
        if [[ -f "build/topo-gen.exe" ]]; then
            PORTABLE_CMD+=(--cli "build/topo-gen.exe")
        else
            PORTABLE_CMD+=(--no-cli)
        fi

        # Add GUI if exists
        if [[ -f "build/topo-gen-gui.exe" ]]; then
            PORTABLE_CMD+=(--gui "build/topo-gen-gui.exe")
        else
            PORTABLE_CMD+=(--no-gui)
        fi

        "${PORTABLE_CMD[@]}"
        log_success "Portable ZIP created"
    elif [[ ! -x "$PORTABLE_SCRIPT" ]]; then
        log_error "Portable ZIP script not found or not executable: $PORTABLE_SCRIPT"
    fi
fi

# Create DEB package (Linux)
if [[ "$CREATE_DEB" == true || "$CREATE_SOURCE_DEB" == true ]]; then
    if [[ "$CREATE_DEB" == true ]]; then
        log_info "Creating DEB package..."
    fi
    DEB_SCRIPT="$SCRIPT_DIR/package/create-deb.sh"
    DEPS_DIR="$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}/topo-gen-${VERSION}/lib"
    [[ ! -d "$DEPS_DIR" ]] && DEPS_DIR="dist/linux-deps"

    DEB_CMD=("$DEB_SCRIPT" --version "$VERSION" --output-dir "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}")

    if [[ "$CREATE_DEB" == true ]]; then
        if [[ -d "$DEPS_DIR" ]]; then
            DEB_CMD+=(--deps-dir "$DEPS_DIR")
        else
            log_warning "Dependencies directory not found, skipping binary DEB"
            CREATE_DEB=false
        fi
    fi

    if [[ "$CREATE_SOURCE_DEB" == true ]]; then
        DEB_CMD+=(--source-deb)
    fi

    if [[ "$CREATE_DEB" == true || "$CREATE_SOURCE_DEB" == true ]]; then
        if [[ -x "$DEB_SCRIPT" ]]; then
            "${DEB_CMD[@]}"
            [[ "$CREATE_DEB" == true ]] && log_success "DEB package created"
            [[ "$CREATE_SOURCE_DEB" == true ]] && log_success "Source DEB created"
        else
            log_warning "DEB script not found or not executable"
        fi
    fi
fi

# Create RPM package (Linux)
if [[ "$CREATE_RPM" == true || "$CREATE_SOURCE_RPM" == true ]]; then
    if [[ "$CREATE_RPM" == true ]]; then
        log_info "Creating RPM package..."
    fi
    RPM_SCRIPT="$SCRIPT_DIR/package/create-rpm.sh"
    DEPS_DIR="$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}/topo-gen-${VERSION}/lib"
    [[ ! -d "$DEPS_DIR" ]] && DEPS_DIR="dist/linux-deps"

    RPM_CMD=("$RPM_SCRIPT" --version "$VERSION" --output-dir "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}")

    if [[ "$CREATE_RPM" == true ]]; then
        if [[ -d "$DEPS_DIR" ]]; then
            RPM_CMD+=(--deps-dir "$DEPS_DIR")
        else
            log_warning "Dependencies directory not found, skipping binary RPM"
            CREATE_RPM=false
        fi
    fi

    if [[ "$CREATE_SOURCE_RPM" == true ]]; then
        RPM_CMD+=(--source-rpm)
    fi

    if [[ "$CREATE_RPM" == true || "$CREATE_SOURCE_RPM" == true ]]; then
        if [[ -x "$RPM_SCRIPT" ]]; then
            "${RPM_CMD[@]}"
            [[ "$CREATE_RPM" == true ]] && log_success "RPM package created"
            [[ "$CREATE_SOURCE_RPM" == true ]] && log_success "Source RPM created"
        else
            log_warning "RPM script not found or not executable"
        fi
    fi
fi

# Create Flatpak manifest
if [[ "$CREATE_FLATPAK" == true ]]; then
    log_info "Generating Flatpak manifest..."
    FLATPAK_SCRIPT="$SCRIPT_DIR/package/create-flatpak.sh"
    SOURCE_TARBALL=$(ls -t "$OUTPUT_DIR/source"/topo-gen-${VERSION}-source-*.tar.gz 2>/dev/null | head -1)

    if [[ -z "$SOURCE_TARBALL" ]]; then
        log_warning "Source tarball not found for Flatpak"
        SOURCE_URL="https://github.com/matthewblock/topo-gen/archive/v${VERSION}.tar.gz"
    else
        SOURCE_URL="$SOURCE_TARBALL"
    fi

    if [[ -x "$FLATPAK_SCRIPT" ]]; then
        "$FLATPAK_SCRIPT" --source-url "$SOURCE_URL" --version "$VERSION" --output-dir "$OUTPUT_DIR/flatpak"
        log_success "Flatpak manifest created"
    fi
fi

# Create AppImage
if [[ "$CREATE_APPIMAGE" == true ]]; then
    log_info "Creating AppImage..."
    APPIMAGE_SCRIPT="$SCRIPT_DIR/package/create-appimage.sh"
    DEPS_DIR="$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}/topo-gen-${VERSION}/lib"
    [[ ! -d "$DEPS_DIR" ]] && DEPS_DIR="dist/linux-deps"

    if [[ -d "$DEPS_DIR" && -x "$APPIMAGE_SCRIPT" ]]; then
        "$APPIMAGE_SCRIPT" --deps-dir "$DEPS_DIR" --version "$VERSION" --output-dir "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"
        log_success "AppImage created"
    else
        log_warning "Cannot create AppImage (missing dependencies or script)"
    fi
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

if [[ "$CREATE_DMG" == true && -d "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}" ]]; then
    echo "DMG installer:"
    ls -lh "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"/*.dmg 2>/dev/null | tail -1 | awk '{print "  " $9, "(" $5 ")"}'
fi

if [[ "$CREATE_PKG" == true && -d "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}" ]]; then
    echo "PKG installer:"
    ls -lh "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"/*.pkg 2>/dev/null | tail -1 | awk '{print "  " $9, "(" $5 ")"}'
fi

if [[ "$CREATE_HOMEBREW" == true && -d "$OUTPUT_DIR/homebrew" ]]; then
    echo "Homebrew formula:"
    ls -lh "$OUTPUT_DIR/homebrew"/*.rb 2>/dev/null | tail -1 | awk '{print "  " $9, "(" $5 ")"}'
fi

if [[ "$CREATE_MSI" == true && -d "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}/wix-sources" ]]; then
    echo "MSI installer (WiX sources):"
    echo "  $OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}/wix-sources/"
fi

if [[ "$CREATE_WINGET" == true && -d "$OUTPUT_DIR/winget" ]]; then
    echo "WinGet manifest:"
    echo "  $OUTPUT_DIR/winget/manifests/"
fi

if [[ "$CREATE_PORTABLE_ZIP" == true ]]; then
    echo "Portable ZIP:"
    ls -lh "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"/*-portable.zip 2>/dev/null | tail -1 | awk '{print "  " $9, "(" $5 ")"}'
fi

if [[ "$CREATE_DEB" == true ]]; then
    echo "DEB package:"
    ls -lh "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"/*.deb 2>/dev/null | tail -1 | awk '{print "  " $9, "(" $5 ")"}'
fi

if [[ "$CREATE_RPM" == true ]]; then
    echo "RPM package:"
    ls -lh "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"/*.rpm 2>/dev/null | tail -1 | awk '{print "  " $9, "(" $5 ")"}'
fi

if [[ "$CREATE_FLATPAK" == true ]]; then
    echo "Flatpak manifest:"
    echo "  $OUTPUT_DIR/flatpak/*.json"
fi

if [[ "$CREATE_APPIMAGE" == true ]]; then
    echo "AppImage:"
    ls -lh "$OUTPUT_DIR/${TARGET_PLATFORM}-${ARCHITECTURE}"/TopoGen-*.AppImage 2>/dev/null | tail -1 | awk '{print "  " $9, "(" $5 ")"}'
fi

echo ""
log_info "To test packages:"

if [[ "$CREATE_BINARY" == true ]]; then
    echo "  Binary:"
    if [[ "$TARGET_PLATFORM" == "windows" ]]; then
        echo "    unzip topo-gen-*.zip && cd topo-gen-*/ && ./bin/topo-gen.exe --help"
    else
        echo "    tar -xzf topo-gen-*.tar.gz && cd topo-gen-*/ && ./bin/topo-gen --help"
    fi
fi

if [[ "$CREATE_DMG" == true ]]; then
    echo "  DMG:"
    echo "    open dist/${TARGET_PLATFORM}-${ARCHITECTURE}/*.dmg"
    echo "    Drag app to Applications, then launch"
fi

if [[ "$CREATE_PKG" == true ]]; then
    echo "  PKG:"
    echo "    open dist/${TARGET_PLATFORM}-${ARCHITECTURE}/*.pkg"
    echo "    Follow installer prompts, then run: topo-gen --help"
fi

if [[ "$CREATE_HOMEBREW" == true ]]; then
    echo "  Homebrew:"
    echo "    brew install --build-from-source dist/homebrew/topo-gen.rb"
fi

if [[ "$CREATE_MSI" == true ]]; then
    echo "  MSI:"
    echo "    On Windows: cd wix-sources && build.bat"
    echo "    Then: Double-click topo-gen.msi"
fi

if [[ "$CREATE_WINGET" == true ]]; then
    echo "  WinGet:"
    echo "    winget validate dist/winget/manifests/..."
    echo "    winget install --manifest dist/winget/manifests/..."
fi

if [[ "$CREATE_PORTABLE_ZIP" == true ]]; then
    echo "  Portable ZIP:"
    echo "    Extract ZIP and run topo-gen.bat or topo-gen-gui.exe"
fi

if [[ "$CREATE_DEB" == true ]]; then
    echo "  DEB:"
    echo "    sudo dpkg -i dist/${TARGET_PLATFORM}-${ARCHITECTURE}/*.deb"
    echo "    sudo apt-get install -f  # Install dependencies"
    echo "    topo-gen --version"
fi

if [[ "$CREATE_RPM" == true ]]; then
    echo "  RPM:"
    echo "    sudo rpm -i dist/${TARGET_PLATFORM}-${ARCHITECTURE}/*.rpm"
    echo "    Or: sudo dnf install dist/${TARGET_PLATFORM}-${ARCHITECTURE}/*.rpm"
    echo "    topo-gen --version"
fi

if [[ "$CREATE_FLATPAK" == true ]]; then
    echo "  Flatpak:"
    echo "    flatpak-builder build-dir dist/flatpak/*.json"
    echo "    flatpak-builder --run build-dir dist/flatpak/*.json topo-gen-gui"
fi

if [[ "$CREATE_APPIMAGE" == true ]]; then
    echo "  AppImage:"
    echo "    chmod +x dist/${TARGET_PLATFORM}-${ARCHITECTURE}/TopoGen-*.AppImage"
    echo "    ./dist/${TARGET_PLATFORM}-${ARCHITECTURE}/TopoGen-*.AppImage"
fi

echo ""
