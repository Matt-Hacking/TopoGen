#!/bin/bash
#
# create-pkg.sh - Create macOS PKG installer
#
# Creates a professional macOS installer package with welcome screens,
# post-install scripts, and proper system integration.
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

Create a macOS PKG installer for Topographic Generator.

Options:
    --cli PATH              CLI executable path (default: build/topo-gen)
    --gui PATH              GUI app bundle path (default: build/topo-gen-gui.app)
    --identifier ID         Package identifier (default: com.matthewblock.topo-gen)
    --version VERSION       Package version (default: from CMakeLists.txt)
    --output-dir DIR        Output directory (default: dist/macos-arm64/)
    --install-location PATH Install location (default: /usr/local)
    --sign-identity ID      Code signing identity (optional)
    --no-cli                Don't include CLI executable
    --no-gui                Don't include GUI application
    --help                  Show this help message

Installation Locations:
    CLI:  /usr/local/bin/topo-gen
    GUI:  /Applications/Topographic Generator.app
    Data: /usr/local/share/topo-gen/

Examples:
    # Basic PKG creation (CLI + GUI)
    $0 --cli build/topo-gen --gui build/topo-gen-gui.app

    # CLI only
    $0 --cli build/topo-gen --no-gui

    # With code signing
    $0 --sign-identity "Developer ID Installer: Your Name"

EOF
    exit 0
}

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PACKAGING_DIR="$PROJECT_ROOT/packaging"

# Extract version from CMakeLists.txt
get_version() {
    grep -E "^project\\(.*VERSION [0-9]+\\.[0-9]+\\.[0-9]+" "$PROJECT_ROOT/CMakeLists.txt" | \
        sed -E 's/.*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/'
}

# Default values
CLI_PATH="$PROJECT_ROOT/build/topo-gen"
GUI_PATH="$PROJECT_ROOT/build/topo-gen-gui.app"
IDENTIFIER="com.matthewblock.topo-gen"
VERSION=$(get_version)
OUTPUT_DIR="$PROJECT_ROOT/dist/macos-arm64"
INSTALL_LOCATION="/usr/local"
SIGN_IDENTITY=""
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
        --identifier)
            IDENTIFIER="$2"
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
        --install-location)
            INSTALL_LOCATION="$2"
            shift 2
            ;;
        --sign-identity)
            SIGN_IDENTITY="$2"
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
if [[ "$INCLUDE_CLI" == true && ! -f "$CLI_PATH" ]]; then
    log_error "CLI executable not found: $CLI_PATH"
    exit 1
fi

if [[ "$INCLUDE_GUI" == true && ! -d "$GUI_PATH" ]]; then
    log_warning "GUI app bundle not found: $GUI_PATH"
    log_info "Proceeding without GUI..."
    INCLUDE_CLI=false
fi

if [[ "$INCLUDE_CLI" == false && "$INCLUDE_GUI" == false ]]; then
    log_error "No components to install"
    exit 1
fi

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

log_info "macOS PKG Installer Creation"
log_info "  Identifier: $IDENTIFIER"
log_info "  Version: $VERSION"
log_info "  Include CLI: $INCLUDE_CLI"
log_info "  Include GUI: $INCLUDE_GUI"
log_info "  Output: $OUTPUT_DIR"
echo ""

# Create temporary directory
TEMP_DIR=$(mktemp -d)
PAYLOAD_DIR="$TEMP_DIR/payload"
SCRIPTS_DIR="$TEMP_DIR/scripts"
RESOURCES_DIR="$TEMP_DIR/resources"

mkdir -p "$PAYLOAD_DIR"
mkdir -p "$SCRIPTS_DIR"
mkdir -p "$RESOURCES_DIR"

log_info "Staging package contents..."

# Stage CLI
if [[ "$INCLUDE_CLI" == true ]]; then
    mkdir -p "$PAYLOAD_DIR/bin"
    cp "$CLI_PATH" "$PAYLOAD_DIR/bin/topo-gen"
    chmod +x "$PAYLOAD_DIR/bin/topo-gen"
    log_success "Staged CLI executable"
fi

# Stage GUI
if [[ "$INCLUDE_GUI" == true ]]; then
    mkdir -p "$PAYLOAD_DIR/Applications"
    cp -R "$GUI_PATH" "$PAYLOAD_DIR/Applications/Topographic Generator.app"
    log_success "Staged GUI application"
fi

# Stage shared data (GDAL data, etc.)
if command -v gdal-config &> /dev/null; then
    GDAL_DATA=$(gdal-config --datadir)
    if [[ -d "$GDAL_DATA" ]]; then
        mkdir -p "$PAYLOAD_DIR/share/topo-gen/gdal"
        cp -R "$GDAL_DATA/"* "$PAYLOAD_DIR/share/topo-gen/gdal/"
        log_success "Staged GDAL data files"
    fi
fi

# Create post-install script
log_info "Creating post-install script..."

cat > "$SCRIPTS_DIR/postinstall" << 'EOF'
#!/bin/bash
# Post-install script for Topographic Generator

set -e

INSTALL_DIR="/usr/local"
BIN_DIR="$INSTALL_DIR/bin"
SHARE_DIR="$INSTALL_DIR/share/topo-gen"

# Create symlinks if they don't exist
if [[ -f "$BIN_DIR/topo-gen" ]]; then
    # Ensure binary is executable
    chmod +x "$BIN_DIR/topo-gen"

    # Add to PATH by updating shell profiles
    for PROFILE in /etc/paths.d /etc/profile /etc/bashrc /etc/zshrc; do
        if [[ -d "$(dirname "$PROFILE")" ]]; then
            if ! grep -q "$BIN_DIR" "$PROFILE" 2>/dev/null; then
                echo "$BIN_DIR" >> "$PROFILE" 2>/dev/null || true
            fi
        fi
    done
fi

# Set GDAL_DATA environment variable
if [[ -d "$SHARE_DIR/gdal" ]]; then
    # Update launchd environment
    launchctl setenv GDAL_DATA "$SHARE_DIR/gdal" 2>/dev/null || true

    # Add to shell profiles
    for PROFILE in "$HOME/.bash_profile" "$HOME/.zshrc"; do
        if [[ -f "$PROFILE" ]]; then
            if ! grep -q "GDAL_DATA" "$PROFILE"; then
                echo "export GDAL_DATA=\"$SHARE_DIR/gdal\"" >> "$PROFILE"
            fi
        fi
    done
fi

# Fix permissions
chown -R root:wheel "$INSTALL_DIR/bin" 2>/dev/null || true
chown -R root:wheel "$SHARE_DIR" 2>/dev/null || true
chmod -R 755 "$INSTALL_DIR/bin" 2>/dev/null || true
chmod -R 755 "$SHARE_DIR" 2>/dev/null || true

# Verify installation
if [[ -f "$BIN_DIR/topo-gen" ]]; then
    echo "Topographic Generator CLI installed successfully"
    "$BIN_DIR/topo-gen" --version || true
fi

if [[ -d "/Applications/Topographic Generator.app" ]]; then
    echo "Topographic Generator GUI installed successfully"
fi

exit 0
EOF

chmod +x "$SCRIPTS_DIR/postinstall"
log_success "Created post-install script"

# Create welcome and readme HTML files
log_info "Creating installer resources..."

cat > "$RESOURCES_DIR/welcome.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Helvetica Neue", Arial, sans-serif; }
        h1 { color: #007AFF; }
        p { line-height: 1.6; }
    </style>
</head>
<body>
    <h1>Welcome to Topographic Generator</h1>
    <p>This installer will install the Topographic Generator on your computer.</p>
    <p><strong>What you're installing:</strong></p>
    <ul>
        <li>Command-line tool (<code>topo-gen</code>)</li>
        <li>GUI application</li>
        <li>Required data files and dependencies</li>
    </ul>
    <p>The installer will place files in:</p>
    <ul>
        <li><code>/usr/local/bin/</code> - Command-line executable</li>
        <li><code>/Applications/</code> - GUI application</li>
        <li><code>/usr/local/share/topo-gen/</code> - Data files</li>
    </ul>
    <p>Click Continue to proceed with the installation.</p>
</body>
</html>
EOF

cat > "$RESOURCES_DIR/readme.html" << 'EOF'
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Helvetica Neue", Arial, sans-serif; }
        h1 { color: #007AFF; }
        h2 { color: #34C759; }
        code { background: #f0f0f0; padding: 2px 6px; border-radius: 3px; }
    </style>
</head>
<body>
    <h1>Topographic Generator</h1>
    <p>High-performance topographic model generator for laser cutting and 3D printing.</p>

    <h2>Getting Started</h2>
    <p>After installation, you can use the command-line tool:</p>
    <pre><code>topo-gen --help</code></pre>

    <p>Or launch the GUI application from your Applications folder.</p>

    <h2>Quick Example</h2>
    <pre><code>topo-gen --upper-left 45.5,-122.7 --lower-right 45.4,-122.6 --num-layers 10</code></pre>

    <h2>Documentation</h2>
    <p>For complete documentation, visit the project repository or run:</p>
    <pre><code>topo-gen --help</code></pre>

    <h2>License</h2>
    <p>MIT License - Copyright © 2025 Matthew Block</p>
</body>
</html>
EOF

log_success "Created installer resources"

# Build component package
log_info "Building component package..."

COMPONENT_PKG="$TEMP_DIR/topo-gen-component.pkg"

pkgbuild \
    --root "$PAYLOAD_DIR" \
    --identifier "$IDENTIFIER" \
    --version "$VERSION" \
    --install-location "$INSTALL_LOCATION" \
    --scripts "$SCRIPTS_DIR" \
    "$COMPONENT_PKG"

log_success "Built component package"

# Build product package
log_info "Building final installer package..."

FINAL_PKG="$OUTPUT_DIR/topo-gen-${VERSION}.pkg"

# Create distribution XML
DISTRIBUTION_XML="$TEMP_DIR/distribution.xml"

cat > "$DISTRIBUTION_XML" << EOF
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="1">
    <title>Topographic Generator</title>
    <welcome file="welcome.html"/>
    <readme file="readme.html"/>
    <pkg-ref id="$IDENTIFIER"/>
    <options customize="never" require-scripts="false" rootVolumeOnly="true"/>
    <choices-outline>
        <line choice="default">
            <line choice="$IDENTIFIER"/>
        </line>
    </choices-outline>
    <choice id="default"/>
    <choice id="$IDENTIFIER" visible="false">
        <pkg-ref id="$IDENTIFIER"/>
    </choice>
    <pkg-ref id="$IDENTIFIER" version="$VERSION" onConclusion="none">topo-gen-component.pkg</pkg-ref>
</installer-gui-script>
EOF

# Build product package
BUILD_CMD=(
    productbuild
    --distribution "$DISTRIBUTION_XML"
    --resources "$RESOURCES_DIR"
    --package-path "$TEMP_DIR"
)

# Add code signing if specified
if [[ -n "$SIGN_IDENTITY" ]]; then
    BUILD_CMD+=(--sign "$SIGN_IDENTITY")
    log_info "Code signing with: $SIGN_IDENTITY"
fi

BUILD_CMD+=("$FINAL_PKG")

"${BUILD_CMD[@]}"

log_success "Built final installer package"

# Clean up temporary files
rm -rf "$TEMP_DIR"

# Verify final package
if [[ -f "$FINAL_PKG" ]]; then
    PKG_SIZE=$(du -h "$FINAL_PKG" | awk '{print $1}')

    echo ""
    log_success "PKG installer created successfully!"
    log_info "Location: $FINAL_PKG"
    log_info "Size: $PKG_SIZE"
    echo ""

    # Show package info
    log_info "Package information:"
    pkgutil --payload-files "$FINAL_PKG" | head -20

    echo ""
    log_info "To test the installer:"
    echo "  open $FINAL_PKG"
    echo ""
    log_info "To distribute:"
    echo "  - Upload to website/GitHub releases"
    echo "  - Users double-click to install"
    echo "  - Requires admin privileges"
    echo ""

    if [[ -z "$SIGN_IDENTITY" ]]; then
        log_warning "Package is not code-signed"
        log_info "Users may see Gatekeeper warnings"
        log_info "To sign: $0 --sign-identity \"Developer ID Installer: Your Name\""
    fi

else
    log_error "Failed to create PKG installer"
    exit 1
fi
