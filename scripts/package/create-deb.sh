#!/bin/bash
#
# create-deb.sh - Create Debian/Ubuntu DEB package
#
# Creates a professional DEB package for Debian, Ubuntu, and derivatives.
# Includes desktop integration, dependency declarations, and proper installation.
#
# Copyright (c) 2025 Matthew Block
# Licensed under the MIT License

set -e

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

# Usage information
usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Create a Debian/Ubuntu DEB package for Topographic Generator.

Options:
    --cli PATH              CLI executable path (default: build/topo-gen)
    --gui PATH              GUI executable path (default: build/topo-gen-gui)
    --deps-dir PATH         Directory with bundled libraries (required)
    --version VERSION       Package version (default: from CMakeLists.txt)
    --output-dir DIR        Output directory (default: dist/linux-x64/)
    --architecture ARCH     Package architecture (default: amd64)
    --maintainer NAME       Maintainer name and email
    --no-cli                Don't include CLI executable
    --no-gui                Don't include GUI application
    --source-deb            Generate source package (.dsc, .debian.tar.xz, .orig.tar.gz)
    --help                  Show this help message

Installation Locations:
    CLI:  /usr/local/bin/topo-gen
    GUI:  /opt/topo-gen/topo-gen-gui
    Libs: /opt/topo-gen/lib/*.so
    Data: /opt/topo-gen/share/gdal/
    Icon: /usr/share/icons/hicolor/256x256/apps/topo-gen.png
    Desktop: /usr/share/applications/topo-gen.desktop

Examples:
    # Basic DEB package (CLI + GUI)
    $0 --cli build/topo-gen --gui build/topo-gen-gui --deps-dir dist/linux-deps

    # CLI only
    $0 --cli build/topo-gen --deps-dir dist/linux-deps --no-gui

    # Custom architecture
    $0 --deps-dir dist/linux-deps --architecture arm64

Notes:
    - Requires dpkg-deb for package creation
    - Automatically declares library dependencies
    - Creates desktop file for GUI integration
    - Sets up PATH and LD_LIBRARY_PATH

EOF
    exit 0
}

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Extract version from CMakeLists.txt
get_version() {
    grep -E "^project\\(.*VERSION [0-9]+\\.[0-9]+\\.[0-9]+" "$PROJECT_ROOT/CMakeLists.txt" | \
        sed -E 's/.*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/'
}

# Default values
CLI_PATH="$PROJECT_ROOT/build/topo-gen"
GUI_PATH="$PROJECT_ROOT/build/topo-gen-gui"
DEPS_DIR=""
VERSION=$(get_version)
OUTPUT_DIR="$PROJECT_ROOT/dist/linux-x64"
ARCHITECTURE="amd64"
MAINTAINER="Matthew Block <matthew@example.com>"
INCLUDE_CLI=true
INCLUDE_GUI=true
CREATE_SOURCE=false

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --cli)
            CLI_PATH="$2"
            shift 2
            ;;
        --gui)
            GUI_PATH="$2"
            shift 2
            ;;
        --deps-dir)
            DEPS_DIR="$2"
            shift 2
            ;;
        --version)
            VERSION="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --architecture)
            ARCHITECTURE="$2"
            shift 2
            ;;
        --maintainer)
            MAINTAINER="$2"
            shift 2
            ;;
        --no-cli)
            INCLUDE_CLI=false
            shift
            ;;
        --no-gui)
            INCLUDE_GUI=false
            shift
            ;;
        --source-deb)
            CREATE_SOURCE=true
            shift
            ;;
        --help)
            usage
            ;;
        *)
            log_error "Unknown argument: $1"
            usage
            ;;
    esac
done

# Validate inputs
# Only require deps-dir for binary packages, not for source-only builds
if [[ "$CREATE_SOURCE" == false ]]; then
    if [[ -z "$DEPS_DIR" ]]; then
        log_error "Dependencies directory is required for binary packages (--deps-dir)"
        log_info "To create source packages only, use --source-deb"
        exit 1
    fi

    if [[ ! -d "$DEPS_DIR" ]]; then
        log_error "Dependencies directory not found: $DEPS_DIR"
        exit 1
    fi
fi

if [[ "$INCLUDE_CLI" == true && ! -f "$CLI_PATH" ]]; then
    log_warning "CLI executable not found: $CLI_PATH"
    log_info "Proceeding without CLI..."
    INCLUDE_CLI=false
fi

if [[ "$INCLUDE_GUI" == true && ! -f "$GUI_PATH" ]]; then
    log_warning "GUI executable not found: $GUI_PATH"
    log_info "Proceeding without GUI..."
    INCLUDE_GUI=false
fi

if [[ "$INCLUDE_CLI" == false && "$INCLUDE_GUI" == false ]]; then
    log_error "No components to package"
    exit 1
fi

# Check for dpkg-deb
if ! command -v dpkg-deb &> /dev/null; then
    log_error "dpkg-deb not found (required for DEB creation)"
    log_info "Install with: sudo apt-get install dpkg-dev"
    exit 1
fi

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

log_info "Debian DEB Package Creation"
log_info "  Version: $VERSION"
log_info "  Architecture: $ARCHITECTURE"
log_info "  Include CLI: $INCLUDE_CLI"
log_info "  Include GUI: $INCLUDE_GUI"
log_info "  Source package only: $CREATE_SOURCE"
log_info "  Output: $OUTPUT_DIR"
echo ""

# Skip binary package creation if only building source packages without deps
if [[ "$CREATE_SOURCE" == true && -z "$DEPS_DIR" ]]; then
    log_info "Skipping binary package creation (source package only)"
else
    # Create temporary package directory
    TEMP_DIR=$(mktemp -d)
    PKG_NAME="topo-gen_${VERSION}_${ARCHITECTURE}"
    PKG_DIR="$TEMP_DIR/$PKG_NAME"

    mkdir -p "$PKG_DIR/DEBIAN"
    mkdir -p "$PKG_DIR/usr/local/bin"
    mkdir -p "$PKG_DIR/opt/topo-gen/lib"
    mkdir -p "$PKG_DIR/opt/topo-gen/share/gdal"
    mkdir -p "$PKG_DIR/usr/share/applications"
    mkdir -p "$PKG_DIR/usr/share/icons/hicolor/256x256/apps"
    mkdir -p "$PKG_DIR/usr/share/man/man1"

    log_info "Staging package contents..."

# Copy CLI executable
if [[ "$INCLUDE_CLI" == true ]]; then
    cp "$CLI_PATH" "$PKG_DIR/usr/local/bin/topo-gen"
    chmod +x "$PKG_DIR/usr/local/bin/topo-gen"
    log_success "Staged CLI executable"
fi

# Copy GUI executable
if [[ "$INCLUDE_GUI" == true ]]; then
    cp "$GUI_PATH" "$PKG_DIR/opt/topo-gen/topo-gen-gui"
    chmod +x "$PKG_DIR/opt/topo-gen/topo-gen-gui"
    log_success "Staged GUI executable"
fi

# Copy shared libraries
log_info "Copying dependency libraries..."
LIB_COUNT=0
for lib in "$DEPS_DIR"/*.so*; do
    if [[ -f "$lib" ]]; then
        cp -P "$lib" "$PKG_DIR/opt/topo-gen/lib/"
        LIB_COUNT=$((LIB_COUNT + 1))
    fi
done
log_success "Copied $LIB_COUNT library files"

# Copy GDAL data if available
if command -v gdal-config &> /dev/null; then
    GDAL_DATA=$(gdal-config --datadir)
    if [[ -d "$GDAL_DATA" ]]; then
        log_info "Copying GDAL data files..."
        cp -R "$GDAL_DATA"/* "$PKG_DIR/opt/topo-gen/share/gdal/"
        log_success "Copied GDAL data"
    fi
fi

# Create desktop file for GUI
if [[ "$INCLUDE_GUI" == true ]]; then
    log_info "Creating desktop file..."
    cat > "$PKG_DIR/usr/share/applications/topo-gen.desktop" << 'EOF'
[Desktop Entry]
Version=1.0
Type=Application
Name=Topographic Generator
GenericName=Topographic Model Generator
Comment=Generate laser-cuttable topographic models from elevation data
Exec=/opt/topo-gen/topo-gen-gui %F
Icon=topo-gen
Terminal=false
Categories=Graphics;Science;Geography;
MimeType=application/json;
Keywords=topography;laser-cutting;3d-printing;maps;elevation;
EOF
    log_success "Created desktop file"

    # Create a simple icon (placeholder - user should replace)
    # For now, create a symbolic link to a default icon or skip
    log_warning "No icon provided - desktop integration may be incomplete"
    log_info "Consider adding a 256x256 PNG icon at /usr/share/icons/hicolor/256x256/apps/topo-gen.png"
fi

# Create wrapper script for GUI (sets LD_LIBRARY_PATH)
if [[ "$INCLUDE_GUI" == true ]]; then
    mkdir -p "$PKG_DIR/usr/local/bin"
    cat > "$PKG_DIR/usr/local/bin/topo-gen-gui" << 'EOF'
#!/bin/bash
# Wrapper script for Topographic Generator GUI
export LD_LIBRARY_PATH="/opt/topo-gen/lib:$LD_LIBRARY_PATH"
export GDAL_DATA="/opt/topo-gen/share/gdal"
exec /opt/topo-gen/topo-gen-gui "$@"
EOF
    chmod +x "$PKG_DIR/usr/local/bin/topo-gen-gui"
    log_success "Created GUI wrapper script"
fi

# Copy and compress man pages
log_info "Installing man pages..."
MAN_SOURCE_DIR="$PROJECT_ROOT/docs/man"
if [[ -f "$MAN_SOURCE_DIR/topo-gen.1" && "$INCLUDE_CLI" == true ]]; then
    gzip -c "$MAN_SOURCE_DIR/topo-gen.1" > "$PKG_DIR/usr/share/man/man1/topo-gen.1.gz"
    chmod 644 "$PKG_DIR/usr/share/man/man1/topo-gen.1.gz"
    log_success "Installed CLI man page"
else
    log_warning "CLI man page not found: $MAN_SOURCE_DIR/topo-gen.1"
fi

if [[ -f "$MAN_SOURCE_DIR/topo-gen-gui.1" && "$INCLUDE_GUI" == true ]]; then
    gzip -c "$MAN_SOURCE_DIR/topo-gen-gui.1" > "$PKG_DIR/usr/share/man/man1/topo-gen-gui.1.gz"
    chmod 644 "$PKG_DIR/usr/share/man/man1/topo-gen-gui.1.gz"
    log_success "Installed GUI man page"
else
    if [[ "$INCLUDE_GUI" == true ]]; then
        log_warning "GUI man page not found: $MAN_SOURCE_DIR/topo-gen-gui.1"
    fi
fi

# Create control file
log_info "Creating control file..."

# Calculate installed size
INSTALLED_SIZE=$(du -sk "$PKG_DIR" | awk '{print $1}')

cat > "$PKG_DIR/DEBIAN/control" << EOF
Package: topo-gen
Version: $VERSION
Section: science
Priority: optional
Architecture: $ARCHITECTURE
Maintainer: $MAINTAINER
Installed-Size: $INSTALLED_SIZE
Depends: libc6 (>= 2.31), libstdc++6 (>= 10), libgcc-s1, libgdal30 | libgdal28, libtbb12 | libtbb2
Suggests: qt6-base-gui | libqt5gui5
Homepage: https://github.com/matthewblock/topo-gen
Description: High-performance topographic model generator
 Topographic Generator creates laser-cuttable topographic models from
 elevation data. It supports SRTM elevation tiles, contour generation,
 and exports to SVG (laser cutting) and STL (3D printing).
 .
 Features:
  - High-performance C++ implementation with CGAL geometry processing
  - Automatic SRTM elevation data downloading
  - Contour polygon generation and simplification
  - OpenStreetMap feature integration (roads, buildings, waterways)
  - Multi-format export (SVG, STL, GeoJSON, Shapefile)
  - Command-line interface and GUI application
EOF

log_success "Created control file"

# Create postinst script
log_info "Creating post-installation script..."

cat > "$PKG_DIR/DEBIAN/postinst" << 'EOF'
#!/bin/bash
# Post-installation script for Topographic Generator

set -e

# Update desktop database for GUI integration
if command -v update-desktop-database &> /dev/null; then
    update-desktop-database /usr/share/applications
fi

# Update icon cache
if command -v gtk-update-icon-cache &> /dev/null; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor 2>/dev/null || true
fi

# Update man page database
if command -v mandb &> /dev/null; then
    mandb -q 2>/dev/null || true
fi

# Create environment file
cat > /etc/profile.d/topo-gen.sh << 'ENVEOF'
# Topographic Generator environment variables
export GDAL_DATA="/opt/topo-gen/share/gdal"
ENVEOF

chmod +x /etc/profile.d/topo-gen.sh

echo "Topographic Generator installed successfully"
echo "Run 'topo-gen --help' to get started"

exit 0
EOF

chmod +x "$PKG_DIR/DEBIAN/postinst"
log_success "Created post-installation script"

# Create prerm script
log_info "Creating pre-removal script..."

cat > "$PKG_DIR/DEBIAN/prerm" << 'EOF'
#!/bin/bash
# Pre-removal script for Topographic Generator

set -e

# Remove environment file
rm -f /etc/profile.d/topo-gen.sh

exit 0
EOF

chmod +x "$PKG_DIR/DEBIAN/prerm"
log_success "Created pre-removal script"

# Create postrm script (cleanup after removal)
cat > "$PKG_DIR/DEBIAN/postrm" << 'EOF'
#!/bin/bash
# Post-removal script for Topographic Generator

set -e

# Update desktop database
if command -v update-desktop-database &> /dev/null; then
    update-desktop-database /usr/share/applications 2>/dev/null || true
fi

# Update icon cache
if command -v gtk-update-icon-cache &> /dev/null; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor 2>/dev/null || true
fi

# Update man page database
if command -v mandb &> /dev/null; then
    mandb -q 2>/dev/null || true
fi

exit 0
EOF

chmod +x "$PKG_DIR/DEBIAN/postrm"
log_success "Created post-removal script"

# Build DEB package
log_info "Building DEB package..."

DEB_FILE="$OUTPUT_DIR/${PKG_NAME}.deb"

dpkg-deb --build "$PKG_DIR" "$DEB_FILE"

log_success "Built DEB package"

# Clean up temporary directory
rm -rf "$TEMP_DIR"

# Verify package
if [[ -f "$DEB_FILE" ]]; then
    DEB_SIZE=$(du -h "$DEB_FILE" | awk '{print $1}')

    echo ""
    log_success "DEB package created successfully!"
    log_info "Location: $DEB_FILE"
    log_info "Size: $DEB_SIZE"
    echo ""

    # Show package info
    log_info "Package information:"
    dpkg-deb --info "$DEB_FILE"

    echo ""
    log_info "Package contents:"
    dpkg-deb --contents "$DEB_FILE" | head -20

    echo ""
    log_info "To install:"
    echo "  sudo dpkg -i $DEB_FILE"
    echo "  sudo apt-get install -f  # Install dependencies if needed"
    echo ""
    log_info "To test:"
    echo "  topo-gen --version"
    echo "  topo-gen-gui"
    echo ""
    log_info "To uninstall:"
    echo "  sudo apt-get remove topo-gen"
    echo ""

else
    log_error "Failed to create DEB package"
    exit 1
fi
fi  # End of binary package creation conditional

# Generate source package if requested
if [[ "$CREATE_SOURCE" == true ]]; then
    echo ""
    log_info "Creating Debian source package..."

    # Check for required tools
    if ! command -v dpkg-source &> /dev/null; then
        log_error "dpkg-source not found (required for source package creation)"
        log_info "Install with: sudo apt-get install dpkg-dev"
        exit 1
    fi

    # Create source package directory
    SRC_TEMP_DIR=$(mktemp -d)
    SRC_NAME="topo-gen-$VERSION"
    SRC_DIR="$SRC_TEMP_DIR/$SRC_NAME"

    # Create orig tarball from git or manual copy
    log_info "Creating upstream source tarball..."

    if command -v git &> /dev/null && [[ -d "$PROJECT_ROOT/.git" ]]; then
        # Use git archive for clean source
        git -C "$PROJECT_ROOT" archive --format=tar --prefix="${SRC_NAME}/" HEAD | gzip > "$SRC_TEMP_DIR/topo-gen_${VERSION}.orig.tar.gz"
        log_success "Created orig.tar.gz from git"
    else
        # Manual copy
        mkdir -p "$SRC_DIR"
        log_info "Copying source files..."
        cp -r "$PROJECT_ROOT"/{src,include,docs,scripts,CMakeLists.txt,vcpkg.json,LICENSE,COPYRIGHT,README.md} "$SRC_DIR/" 2>/dev/null || true
        cd "$SRC_TEMP_DIR"
        tar -czf "topo-gen_${VERSION}.orig.tar.gz" "$SRC_NAME"
        cd "$PROJECT_ROOT"
        rm -rf "$SRC_DIR"
        log_success "Created orig.tar.gz from source tree"
    fi

    # Extract orig tarball for debian packaging
    cd "$SRC_TEMP_DIR"
    tar -xzf "topo-gen_${VERSION}.orig.tar.gz"

    # Create debian directory structure
    DEBIAN_DIR="$SRC_DIR/debian"
    mkdir -p "$DEBIAN_DIR"
    mkdir -p "$DEBIAN_DIR/source"

    # Create debian/source/format
    echo "3.0 (quilt)" > "$DEBIAN_DIR/source/format"

    # Create debian/control
    cat > "$DEBIAN_DIR/control" << EOF
Source: topo-gen
Section: science
Priority: optional
Maintainer: $MAINTAINER
Build-Depends: debhelper-compat (= 13),
               cmake (>= 3.20),
               g++ (>= 10),
               libgdal-dev,
               libeigen3-dev,
               libtbb-dev,
               libfreetype-dev,
               libcurl4-openssl-dev,
               libssl-dev,
               zlib1g-dev,
               libgmp-dev,
               libmpfr-dev,
               nlohmann-json3-dev
Standards-Version: 4.6.0
Homepage: https://github.com/matthewblock/topo-gen

Package: topo-gen
Architecture: any
Depends: \${shlibs:Depends}, \${misc:Depends}
Description: High-performance topographic model generator
 Topographic Generator creates laser-cuttable topographic models from
 elevation data. It supports SRTM elevation tiles, contour generation,
 and exports to SVG (laser cutting) and STL (3D printing).
 .
 Features:
  - High-performance C++ implementation with CGAL geometry processing
  - Automatic SRTM elevation data downloading
  - Contour polygon generation and simplification
  - OpenStreetMap feature integration (roads, buildings, waterways)
  - Multi-format export (SVG, STL, GeoJSON, Shapefile)
  - Command-line interface and GUI application
EOF

    # Create debian/changelog
    cat > "$DEBIAN_DIR/changelog" << EOF
topo-gen ($VERSION-1) unstable; urgency=low

  * New upstream release

 -- $MAINTAINER  $(date -R)
EOF

    # Create debian/rules
    cat > "$DEBIAN_DIR/rules" << 'EOF'
#!/usr/bin/make -f

%:
	dh $@ --buildsystem=cmake

override_dh_auto_configure:
	dh_auto_configure -- -DCMAKE_BUILD_TYPE=Release

override_dh_auto_install:
	dh_auto_install --destdir=debian/topo-gen
EOF
    chmod +x "$DEBIAN_DIR/rules"

    # Create debian/copyright
    cp "$PROJECT_ROOT/LICENSE" "$DEBIAN_DIR/copyright" 2>/dev/null || echo "MIT License" > "$DEBIAN_DIR/copyright"

    # Create debian/compat
    echo "13" > "$DEBIAN_DIR/compat"

    # Build source package
    log_info "Building source package with dpkg-source..."
    cd "$SRC_DIR"
    dpkg-source -b .

    # Move source package files to output directory
    mv "$SRC_TEMP_DIR"/*.dsc "$OUTPUT_DIR/" 2>/dev/null || true
    mv "$SRC_TEMP_DIR"/*.debian.tar.xz "$OUTPUT_DIR/" 2>/dev/null || true
    mv "$SRC_TEMP_DIR"/*.orig.tar.gz "$OUTPUT_DIR/" 2>/dev/null || true

    # Clean up
    rm -rf "$SRC_TEMP_DIR"

    echo ""
    log_success "Debian source package created successfully!"
    log_info "Source package files:"
    ls -lh "$OUTPUT_DIR"/topo-gen_${VERSION}* | awk '{print "  " $9, "(" $5 ")"}'
    echo ""
    log_info "To build binary package from source:"
    echo "  cd /path/to/build/dir"
    echo "  dpkg-source -x $OUTPUT_DIR/topo-gen_${VERSION}-1.dsc"
    echo "  cd topo-gen-$VERSION"
    echo "  dpkg-buildpackage -us -uc"
    echo ""
fi
