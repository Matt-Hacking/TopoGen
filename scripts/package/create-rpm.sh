#!/bin/bash
#
# create-rpm.sh - Create Fedora/RHEL RPM package
#
# Creates a professional RPM package for Fedora, RHEL, openSUSE, and derivatives.
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

Create a Fedora/RHEL/openSUSE RPM package for Topographic Generator.

Options:
    --cli PATH              CLI executable path (default: build/topo-gen)
    --gui PATH              GUI executable path (default: build/topo-gen-gui)
    --deps-dir PATH         Directory with bundled libraries (required)
    --version VERSION       Package version (default: from CMakeLists.txt)
    --output-dir DIR        Output directory (default: dist/linux-x64/)
    --architecture ARCH     Package architecture (default: x86_64)
    --maintainer NAME       Maintainer name and email
    --no-cli                Don't include CLI executable
    --no-gui                Don't include GUI application
    --help                  Show this help message

Installation Locations:
    CLI:  /usr/local/bin/topo-gen
    GUI:  /opt/topo-gen/topo-gen-gui
    Libs: /opt/topo-gen/lib/*.so
    Data: /opt/topo-gen/share/gdal/
    Icon: /usr/share/icons/hicolor/256x256/apps/topo-gen.png
    Desktop: /usr/share/applications/topo-gen.desktop

Examples:
    # Basic RPM package (CLI + GUI)
    $0 --cli build/topo-gen --gui build/topo-gen-gui --deps-dir dist/linux-deps

    # CLI only
    $0 --cli build/topo-gen --deps-dir dist/linux-deps --no-gui

    # Custom architecture
    $0 --deps-dir dist/linux-deps --architecture aarch64

Notes:
    - Requires rpmbuild for package creation
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
ARCHITECTURE="x86_64"
MAINTAINER="Matthew Block <matthew@example.com>"
INCLUDE_CLI=true
INCLUDE_GUI=true

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
if [[ -z "$DEPS_DIR" ]]; then
    log_error "Dependencies directory is required (--deps-dir)"
    exit 1
fi

if [[ ! -d "$DEPS_DIR" ]]; then
    log_error "Dependencies directory not found: $DEPS_DIR"
    exit 1
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

# Check for rpmbuild
if ! command -v rpmbuild &> /dev/null; then
    log_error "rpmbuild not found (required for RPM creation)"
    log_info "Install with: sudo dnf install rpm-build  (Fedora/RHEL)"
    log_info "          or: sudo zypper install rpm-build  (openSUSE)"
    exit 1
fi

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

log_info "Fedora/RHEL RPM Package Creation"
log_info "  Version: $VERSION"
log_info "  Architecture: $ARCHITECTURE"
log_info "  Include CLI: $INCLUDE_CLI"
log_info "  Include GUI: $INCLUDE_GUI"
log_info "  Output: $OUTPUT_DIR"
echo ""

# Create rpmbuild directory structure
TEMP_DIR=$(mktemp -d)
RPMBUILD_DIR="$TEMP_DIR/rpmbuild"

mkdir -p "$RPMBUILD_DIR"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}
mkdir -p "$RPMBUILD_DIR/BUILD/topo-gen-$VERSION"

BUILDROOT="$RPMBUILD_DIR/BUILD/topo-gen-$VERSION"

mkdir -p "$BUILDROOT/usr/local/bin"
mkdir -p "$BUILDROOT/opt/topo-gen/lib"
mkdir -p "$BUILDROOT/opt/topo-gen/share/gdal"
mkdir -p "$BUILDROOT/usr/share/applications"
mkdir -p "$BUILDROOT/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$BUILDROOT/etc/profile.d"

log_info "Staging package contents..."

# Copy CLI executable
if [[ "$INCLUDE_CLI" == true ]]; then
    cp "$CLI_PATH" "$BUILDROOT/usr/local/bin/topo-gen"
    chmod +x "$BUILDROOT/usr/local/bin/topo-gen"
    log_success "Staged CLI executable"
fi

# Copy GUI executable
if [[ "$INCLUDE_GUI" == true ]]; then
    cp "$GUI_PATH" "$BUILDROOT/opt/topo-gen/topo-gen-gui"
    chmod +x "$BUILDROOT/opt/topo-gen/topo-gen-gui"
    log_success "Staged GUI executable"
fi

# Copy shared libraries
log_info "Copying dependency libraries..."
LIB_COUNT=0
for lib in "$DEPS_DIR"/*.so*; do
    if [[ -f "$lib" ]]; then
        cp -P "$lib" "$BUILDROOT/opt/topo-gen/lib/"
        LIB_COUNT=$((LIB_COUNT + 1))
    fi
done
log_success "Copied $LIB_COUNT library files"

# Copy GDAL data if available
if command -v gdal-config &> /dev/null; then
    GDAL_DATA=$(gdal-config --datadir)
    if [[ -d "$GDAL_DATA" ]]; then
        log_info "Copying GDAL data files..."
        cp -R "$GDAL_DATA"/* "$BUILDROOT/opt/topo-gen/share/gdal/"
        log_success "Copied GDAL data"
    fi
fi

# Create desktop file for GUI
if [[ "$INCLUDE_GUI" == true ]]; then
    log_info "Creating desktop file..."
    cat > "$BUILDROOT/usr/share/applications/topo-gen.desktop" << 'EOF'
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
fi

# Create wrapper script for GUI
if [[ "$INCLUDE_GUI" == true ]]; then
    cat > "$BUILDROOT/usr/local/bin/topo-gen-gui" << 'EOF'
#!/bin/bash
# Wrapper script for Topographic Generator GUI
export LD_LIBRARY_PATH="/opt/topo-gen/lib:$LD_LIBRARY_PATH"
export GDAL_DATA="/opt/topo-gen/share/gdal"
exec /opt/topo-gen/topo-gen-gui "$@"
EOF
    chmod +x "$BUILDROOT/usr/local/bin/topo-gen-gui"
    log_success "Created GUI wrapper script"
fi

# Create environment file
cat > "$BUILDROOT/etc/profile.d/topo-gen.sh" << 'EOF'
# Topographic Generator environment variables
export GDAL_DATA="/opt/topo-gen/share/gdal"
EOF

log_success "Created environment file"

# Create spec file
log_info "Creating RPM spec file..."

cat > "$RPMBUILD_DIR/SPECS/topo-gen.spec" << EOF
Name:           topo-gen
Version:        $VERSION
Release:        1%{?dist}
Summary:        High-performance topographic model generator
License:        MIT
URL:            https://github.com/matthewblock/topo-gen
BuildArch:      $ARCHITECTURE

Requires:       glibc >= 2.31
Requires:       libstdc++ >= 10
Requires:       gdal-libs
Requires:       tbb
%if "$INCLUDE_GUI" == "true"
Suggests:       qt6-qtbase-gui
%endif

%description
Topographic Generator creates laser-cuttable topographic models from
elevation data. It supports SRTM elevation tiles, contour generation,
and exports to SVG (laser cutting) and STL (3D printing).

Features:
 - High-performance C++ implementation with CGAL geometry processing
 - Automatic SRTM elevation data downloading
 - Contour polygon generation and simplification
 - OpenStreetMap feature integration (roads, buildings, waterways)
 - Multi-format export (SVG, STL, GeoJSON, Shapefile)
 - Command-line interface and GUI application

%prep
# No prep needed (binary package)

%build
# No build needed (binary package)

%install
rm -rf \$RPM_BUILD_ROOT
mkdir -p \$RPM_BUILD_ROOT
cp -r $BUILDROOT/* \$RPM_BUILD_ROOT/

%files
%defattr(-,root,root,-)
EOF

# Add file list based on what's included
if [[ "$INCLUDE_CLI" == true ]]; then
    echo "/usr/local/bin/topo-gen" >> "$RPMBUILD_DIR/SPECS/topo-gen.spec"
fi

if [[ "$INCLUDE_GUI" == true ]]; then
    cat >> "$RPMBUILD_DIR/SPECS/topo-gen.spec" << 'EOF'
/usr/local/bin/topo-gen-gui
/opt/topo-gen/topo-gen-gui
/usr/share/applications/topo-gen.desktop
EOF
fi

cat >> "$RPMBUILD_DIR/SPECS/topo-gen.spec" << 'EOF'
/opt/topo-gen/lib/
/opt/topo-gen/share/gdal/
/etc/profile.d/topo-gen.sh

%post
# Update desktop database
if command -v update-desktop-database &> /dev/null; then
    update-desktop-database /usr/share/applications 2>/dev/null || :
fi

# Update icon cache
if command -v gtk-update-icon-cache &> /dev/null; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor 2>/dev/null || :
fi

echo "Topographic Generator installed successfully"
echo "Run 'topo-gen --help' to get started"

%postun
# Update desktop database on removal
if command -v update-desktop-database &> /dev/null; then
    update-desktop-database /usr/share/applications 2>/dev/null || :
fi

# Update icon cache on removal
if command -v gtk-update-icon-cache &> /dev/null; then
    gtk-update-icon-cache -q -t -f /usr/share/icons/hicolor 2>/dev/null || :
fi

%changelog
* $(date "+%a %b %d %Y") $MAINTAINER - $VERSION-1
- Initial RPM package

EOF

log_success "Created RPM spec file"

# Build RPM package
log_info "Building RPM package..."

rpmbuild --define "_topdir $RPMBUILD_DIR" \
         --define "_rpmdir $OUTPUT_DIR" \
         -bb "$RPMBUILD_DIR/SPECS/topo-gen.spec"

log_success "Built RPM package"

# Find generated RPM
RPM_FILE=$(find "$OUTPUT_DIR" -name "topo-gen-${VERSION}*.rpm" | head -1)

# Clean up temporary directory
rm -rf "$TEMP_DIR"

# Verify package
if [[ -f "$RPM_FILE" ]]; then
    RPM_SIZE=$(du -h "$RPM_FILE" | awk '{print $1}')

    echo ""
    log_success "RPM package created successfully!"
    log_info "Location: $RPM_FILE"
    log_info "Size: $RPM_SIZE"
    echo ""

    # Show package info
    log_info "Package information:"
    rpm -qip "$RPM_FILE"

    echo ""
    log_info "Package contents:"
    rpm -qlp "$RPM_FILE" | head -20

    echo ""
    log_info "To install:"
    echo "  sudo rpm -i $RPM_FILE"
    echo "  Or: sudo dnf install $RPM_FILE  (Fedora/RHEL)"
    echo "  Or: sudo zypper install $RPM_FILE  (openSUSE)"
    echo ""
    log_info "To test:"
    echo "  topo-gen --version"
    echo "  topo-gen-gui"
    echo ""
    log_info "To uninstall:"
    echo "  sudo rpm -e topo-gen"
    echo "  Or: sudo dnf remove topo-gen"
    echo ""

else
    log_error "Failed to create RPM package"
    exit 1
fi
