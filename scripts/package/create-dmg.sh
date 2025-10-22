#!/bin/bash
#
# create-dmg.sh - Create macOS DMG installer
#
# Creates a professional disk image with custom background,
# window layout, and drag-to-Applications functionality.
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
Usage: $0 [OPTIONS] <app_bundle_path>

Create a macOS DMG installer for Topographic Generator.

Options:
    --volume-name NAME      Volume name (default: Topographic Generator)
    --dmg-name NAME         Output DMG filename (default: topo-gen-{version}.dmg)
    --output-dir DIR        Output directory (default: dist/macos-arm64/)
    --background IMAGE      Custom background image
    --window-size WxH       Window size (default: 600x400)
    --icon-size SIZE        Icon size (default: 128)
    --app-position X,Y      App icon position (default: 150,200)
    --link-position X,Y     Applications link position (default: 450,200)
    --help                  Show this help message

Examples:
    # Basic DMG creation
    $0 build/topo-gen-gui.app

    # Custom output
    $0 --output-dir dist/macos-arm64 --dmg-name MyApp.dmg build/topo-gen-gui.app

    # Custom layout
    $0 --window-size 800x600 --icon-size 160 build/topo-gen-gui.app

EOF
    exit 0
}

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
PACKAGING_DIR="$PROJECT_ROOT/packaging"

# Default values
VOLUME_NAME="Topographic Generator"
DMG_NAME=""
OUTPUT_DIR="$PROJECT_ROOT/dist/macos-arm64"
BACKGROUND_IMAGE=""
WINDOW_WIDTH=600
WINDOW_HEIGHT=400
ICON_SIZE=128
APP_POSITION_X=150
APP_POSITION_Y=200
LINK_POSITION_X=450
LINK_POSITION_Y=200
APP_BUNDLE=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --volume-name)
            VOLUME_NAME="$2"
            shift 2
            ;;
        --dmg-name)
            DMG_NAME="$2"
            shift 2
            ;;
        --output-dir)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        --background)
            BACKGROUND_IMAGE="$2"
            shift 2
            ;;
        --window-size)
            IFS='x' read -r WINDOW_WIDTH WINDOW_HEIGHT <<< "$2"
            shift 2
            ;;
        --icon-size)
            ICON_SIZE="$2"
            shift 2
            ;;
        --app-position)
            IFS=',' read -r APP_POSITION_X APP_POSITION_Y <<< "$2"
            shift 2
            ;;
        --link-position)
            IFS=',' read -r LINK_POSITION_X LINK_POSITION_Y <<< "$2"
            shift 2
            ;;
        --help)
            usage
            ;;
        *)
            if [[ -z "$APP_BUNDLE" ]]; then
                APP_BUNDLE="$1"
            else
                log_error "Unknown argument: $1"
                exit 1
            fi
            shift
            ;;
    esac
done

# Validate inputs
if [[ -z "$APP_BUNDLE" ]]; then
    log_error "No app bundle specified"
    usage
fi

if [[ ! -d "$APP_BUNDLE" ]]; then
    log_error "App bundle does not exist: $APP_BUNDLE"
    exit 1
fi

# Get version from app bundle Info.plist or use default
VERSION="0.22.001"
if [[ -f "$APP_BUNDLE/Contents/Info.plist" ]]; then
    VERSION=$(defaults read "$APP_BUNDLE/Contents/Info.plist" CFBundleShortVersionString 2>/dev/null || echo "0.22.001")
fi

# Set DMG name if not specified
if [[ -z "$DMG_NAME" ]]; then
    DMG_NAME="topo-gen-${VERSION}.dmg"
fi

# Ensure output directory exists
mkdir -p "$OUTPUT_DIR"

log_info "macOS DMG Creation"
log_info "  App Bundle: $APP_BUNDLE"
log_info "  Volume Name: $VOLUME_NAME"
log_info "  DMG Name: $DMG_NAME"
log_info "  Output: $OUTPUT_DIR"
log_info "  Version: $VERSION"
echo ""

# Create temporary directory for DMG contents
TEMP_DIR=$(mktemp -d)
DMG_STAGING="$TEMP_DIR/dmg-staging"
mkdir -p "$DMG_STAGING"

log_info "Staging DMG contents..."

# Copy app bundle to staging directory
cp -R "$APP_BUNDLE" "$DMG_STAGING/"
APP_NAME=$(basename "$APP_BUNDLE")

log_success "Copied application bundle"

# Create Applications folder symlink
ln -s /Applications "$DMG_STAGING/Applications"
log_success "Created Applications symlink"

# Copy or create background image
if [[ -n "$BACKGROUND_IMAGE" && -f "$BACKGROUND_IMAGE" ]]; then
    mkdir -p "$DMG_STAGING/.background"
    cp "$BACKGROUND_IMAGE" "$DMG_STAGING/.background/background.png"
    log_success "Added custom background image"
elif [[ -f "$PACKAGING_DIR/macos/dmg-template/background.png" ]]; then
    mkdir -p "$DMG_STAGING/.background"
    cp "$PACKAGING_DIR/macos/dmg-template/background.png" "$DMG_STAGING/.background/background.png"
    log_success "Added default background image"
fi

# Create temporary DMG
TEMP_DMG="$TEMP_DIR/temp.dmg"
FINAL_DMG="$OUTPUT_DIR/$DMG_NAME"

log_info "Creating temporary DMG..."

# Calculate size (app bundle size + 50MB overhead)
APP_SIZE=$(du -sm "$DMG_STAGING" | awk '{print $1}')
DMG_SIZE=$((APP_SIZE + 50))

hdiutil create -srcfolder "$DMG_STAGING" \
    -volname "$VOLUME_NAME" \
    -fs HFS+ \
    -fsargs "-c c=64,a=16,e=16" \
    -format UDRW \
    -size ${DMG_SIZE}m \
    "$TEMP_DMG"

log_success "Created temporary DMG"

# Mount the DMG
log_info "Mounting DMG for customization..."
MOUNT_DIR=$(hdiutil attach -readwrite -noverify -noautoopen "$TEMP_DMG" | egrep '^/dev/' | sed 1q | awk '{print $3}')

if [[ -z "$MOUNT_DIR" ]]; then
    log_error "Failed to mount DMG"
    rm -rf "$TEMP_DIR"
    exit 1
fi

log_success "Mounted at: $MOUNT_DIR"

# Wait for mount to settle
sleep 2

# Set custom icon if available
if [[ -f "$PACKAGING_DIR/macos/dmg-template/VolumeIcon.icns" ]]; then
    cp "$PACKAGING_DIR/macos/dmg-template/VolumeIcon.icns" "$MOUNT_DIR/.VolumeIcon.icns"
    SetFile -c icnC "$MOUNT_DIR/.VolumeIcon.icns"
    SetFile -a C "$MOUNT_DIR"
fi

# Create .DS_Store for window layout
log_info "Configuring window layout..."

# Use AppleScript to configure Finder window
osascript << EOF
tell application "Finder"
    tell disk "$VOLUME_NAME"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {100, 100, $((100 + WINDOW_WIDTH)), $((100 + WINDOW_HEIGHT))}
        set viewOptions to the icon view options of container window
        set arrangement of viewOptions to not arranged
        set icon size of viewOptions to $ICON_SIZE
        set background picture of viewOptions to file ".background:background.png"

        -- Position icons
        set position of item "$APP_NAME" of container window to {$APP_POSITION_X, $APP_POSITION_Y}
        set position of item "Applications" of container window to {$LINK_POSITION_X, $LINK_POSITION_Y}

        -- Update and close
        update without registering applications
        delay 2
        close
    end tell
end tell
EOF

log_success "Window layout configured"

# Ensure .DS_Store is written
sync
sleep 2

# Unmount the DMG
log_info "Unmounting DMG..."
hdiutil detach "$MOUNT_DIR" -quiet -force || true
sleep 2

# Convert to compressed, read-only final DMG
log_info "Creating final compressed DMG..."

# Remove existing final DMG if present
rm -f "$FINAL_DMG"

hdiutil convert "$TEMP_DMG" \
    -format UDZO \
    -imagekey zlib-level=9 \
    -o "$FINAL_DMG"

log_success "Created compressed DMG"

# Clean up temporary files
rm -rf "$TEMP_DIR"

# Verify final DMG
if [[ -f "$FINAL_DMG" ]]; then
    DMG_SIZE=$(du -h "$FINAL_DMG" | awk '{print $1}')

    echo ""
    log_success "DMG created successfully!"
    log_info "Location: $FINAL_DMG"
    log_info "Size: $DMG_SIZE"
    echo ""

    # Test mounting
    log_info "Verifying DMG..."
    if hdiutil verify "$FINAL_DMG" -quiet; then
        log_success "DMG verification passed"
    else
        log_warning "DMG verification failed (may still be usable)"
    fi

    echo ""
    log_info "To test the DMG:"
    echo "  open $FINAL_DMG"
    echo ""
    log_info "To distribute:"
    echo "  - Upload to website/GitHub releases"
    echo "  - Users can download and drag app to Applications"
    echo ""

else
    log_error "Failed to create DMG"
    exit 1
fi
