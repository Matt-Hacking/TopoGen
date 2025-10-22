#!/bin/bash
#
# create-flatpak.sh - Generate Flatpak manifest
#
# Creates Flatpak manifest for universal Linux distribution via Flathub.
#
# Copyright (c) 2025 Matthew Block
# Licensed under the MIT License

set -e

RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; BLUE='\033[0;34m'; NC='\033[0m'
log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warning() { echo -e "${YELLOW}[WARNING]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Generate Flatpak manifest for Topographic Generator.

Options:
    --source-url URL        Source tarball URL (required)
    --version VERSION       Package version (default: from CMakeLists.txt)
    --output-dir DIR        Output directory (default: dist/flatpak/)
    --app-id ID             Application ID (default: com.matthewblock.TopoGen)
    --help                  Show this help message

Examples:
    $0 --source-url https://github.com/user/topo-gen/archive/v0.22.001.tar.gz

Notes:
    - Creates manifest for submission to Flathub
    - Requires Flatpak builder for testing
    - See Flathub documentation for submission process
EOF
    exit 0
}

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

get_version() {
    grep -E "^project\\(.*VERSION [0-9]+\\.[0-9]+\\.[0-9]+" "$PROJECT_ROOT/CMakeLists.txt" | \
        sed -E 's/.*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/'
}

SOURCE_URL=""
VERSION=$(get_version)
OUTPUT_DIR="$PROJECT_ROOT/dist/flatpak"
APP_ID="com.matthewblock.TopoGen"

while [[ $# -gt 0 ]]; do
    case $1 in
        --source-url) SOURCE_URL="$2"; shift 2 ;;
        --version) VERSION="$2"; shift 2 ;;
        --output-dir) OUTPUT_DIR="$2"; shift 2 ;;
        --app-id) APP_ID="$2"; shift 2 ;;
        --help) usage ;;
        *) log_error "Unknown argument: $1"; usage ;;
    esac
done

if [[ -z "$SOURCE_URL" ]]; then
    log_error "Source URL is required"
    usage
fi

mkdir -p "$OUTPUT_DIR"

log_info "Flatpak Manifest Generation"
log_info "  App ID: $APP_ID"
log_info "  Version: $VERSION"
log_info "  Output: $OUTPUT_DIR"
echo ""

# Generate Flatpak manifest
cat > "$OUTPUT_DIR/${APP_ID}.json" << EOF
{
  "app-id": "$APP_ID",
  "runtime": "org.freedesktop.Platform",
  "runtime-version": "23.08",
  "sdk": "org.freedesktop.Sdk",
  "command": "topo-gen-gui",
  "finish-args": [
    "--share=ipc",
    "--socket=x11",
    "--socket=wayland",
    "--device=dri",
    "--filesystem=home"
  ],
  "modules": [
    {
      "name": "gdal",
      "buildsystem": "cmake-ninja",
      "config-opts": ["-DCMAKE_BUILD_TYPE=Release"],
      "sources": [{"type": "archive", "url": "https://github.com/OSGeo/gdal/releases/download/v3.8.0/gdal-3.8.0.tar.gz"}]
    },
    {
      "name": "cgal",
      "buildsystem": "cmake-ninja",
      "sources": [{"type": "archive", "url": "https://github.com/CGAL/cgal/releases/download/v5.6/CGAL-5.6.tar.xz"}]
    },
    {
      "name": "topo-gen",
      "buildsystem": "cmake-ninja",
      "config-opts": ["-DCMAKE_BUILD_TYPE=Release"],
      "sources": [{"type": "archive", "url": "$SOURCE_URL"}],
      "post-install": [
        "install -Dm644 packaging/linux/topo-gen.desktop /app/share/applications/${APP_ID}.desktop",
        "install -Dm644 packaging/linux/topo-gen.png /app/share/icons/hicolor/256x256/apps/${APP_ID}.png"
      ]
    }
  ]
}
EOF

log_success "Generated Flatpak manifest"

# Create README
cat > "$OUTPUT_DIR/README.md" << 'EOF'
# Flatpak Manifest for Topographic Generator

## Local Testing

```bash
# Install flatpak-builder
sudo apt install flatpak-builder  # Debian/Ubuntu
sudo dnf install flatpak-builder  # Fedora

# Add Flathub repository
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

# Build application
flatpak-builder --force-clean build-dir com.matthewblock.TopoGen.json

# Test run
flatpak-builder --run build-dir com.matthewblock.TopoGen.json topo-gen-gui

# Install locally
flatpak-builder --user --install --force-clean build-dir com.matthewblock.TopoGen.json
```

## Flathub Submission

1. Fork https://github.com/flathub/flathub
2. Create new repository for your app
3. Add manifest and test locally
4. Submit pull request to Flathub
5. Follow review process

See: https://docs.flathub.org/docs/for-app-authors/submission/
EOF

log_success "Created README.md"

echo ""
log_success "Flatpak manifest created: $OUTPUT_DIR/${APP_ID}.json"
log_info "Test with: flatpak-builder build-dir $OUTPUT_DIR/${APP_ID}.json"
