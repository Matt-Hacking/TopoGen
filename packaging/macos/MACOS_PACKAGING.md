# macOS Packaging Guide

Complete guide for creating macOS distribution packages (DMG, PKG, Homebrew).

## Prerequisites

- macOS 12+ (Monterey or later)
- Xcode Command Line Tools installed
- Built executables in `build/`
- Git repository initialized

## Quick Start

### Create All macOS Packages

```bash
# Build everything and create all macOS packages
./scripts/deploy.sh --all-macos --clean-build
```

This creates:
- Source tarball (`dist/source/`)
- Binary package (`dist/macos-arm64/`)
- DMG installer (`dist/macos-arm64/`)
- PKG installer (`dist/macos-arm64/`)
- Homebrew formula (`dist/homebrew/`)

### Individual Package Types

#### DMG (Drag-to-Install)
```bash
# Create DMG installer
./scripts/deploy.sh --dmg

# Or use script directly
./scripts/package/create-dmg.sh build/topo-gen-gui.app
```

**Output**: `dist/macos-arm64/topo-gen-{version}.dmg`

**Use case**: GUI application distribution, easy installation for end users

#### PKG (System Installer)
```bash
# Create PKG installer
./scripts/deploy.sh --pkg

# Or use script directly with options
./scripts/package/create-pkg.sh \
    --cli build/topo-gen \
    --gui build/topo-gen-gui.app \
    --output-dir dist/macos-arm64
```

**Output**: `dist/macos-arm64/topo-gen-{version}.pkg`

**Use case**: System-wide CLI installation, enterprise deployment

#### Homebrew Formula
```bash
# Generate Homebrew formula
./scripts/deploy.sh --homebrew

# Or use script directly
./scripts/package/create-homebrew.sh \
    --tarball https://github.com/user/topo-gen/archive/v0.22.001.tar.gz \
    --version 0.22.001
```

**Output**: `dist/homebrew/topo-gen.rb`

**Use case**: Command-line tool distribution via `brew install`

## DMG Customization

### Custom Background Image

1. Create 600x400px PNG image with installation instructions
2. Save as `packaging/macos/dmg-template/background.png`
3. Re-run DMG creation

**Example Design:**
```
[App Icon]  ---------->  [Applications Folder]
   (left)                     (right)

      "Drag to install"
```

See `packaging/macos/dmg-template/README.md` for detailed instructions.

### Custom Layout

Modify the create-dmg.sh script parameters:

```bash
./scripts/package/create-dmg.sh \
    --window-size 800x600 \
    --icon-size 160 \
    --app-position 200,250 \
    --link-position 600,250 \
    build/topo-gen-gui.app
```

### Volume Icon

Place custom `VolumeIcon.icns` in `packaging/macos/dmg-template/`

## PKG Customization

### Post-Install Scripts

The PKG installer automatically:
- Installs CLI to `/usr/local/bin/topo-gen`
- Installs GUI to `/Applications/Topographic Generator.app`
- Sets up GDAL_DATA environment variable
- Adds `/usr/local/bin` to PATH

Modify `scripts/package/create-pkg.sh` line ~80 to customize the post-install script.

### Welcome/Readme Screens

Edit HTML files generated in the PKG script:
- Welcome screen (line ~290)
- Readme screen (line ~310)

### Code Signing

```bash
./scripts/package/create-pkg.sh \
    --sign-identity "Developer ID Installer: Your Name (TEAMID)"
```

**Note**: Requires Apple Developer Program membership ($99/year)

## Homebrew Distribution

### Option 1: Custom Tap (Recommended for initial release)

```bash
# 1. Create tap repository
mkdir -p ~/homebrew-topo-gen/Formula
cp dist/homebrew/topo-gen.rb ~/homebrew-topo-gen/Formula/

cd ~/homebrew-topo-gen
git init
git add .
git commit -m "Add topo-gen formula"
git remote add origin https://github.com/yourusername/homebrew-topo-gen
git push -u origin main

# 2. Users install with:
brew tap yourusername/topo-gen
brew install topo-gen
```

### Option 2: Homebrew Core (For mature projects)

1. Fork [homebrew-core](https://github.com/Homebrew/homebrew-core)
2. Add formula to `Formula/t/topo-gen.rb`
3. Submit pull request
4. Follow [Homebrew contribution guidelines](https://docs.brew.sh/How-To-Open-a-Homebrew-Pull-Request)

**Requirements for core**:
- Stable release (1.0+)
- Active maintenance
- Documented, tested codebase
- No duplicate functionality

### Creating Bottles (Binary Packages)

Speed up installation with pre-built binaries:

```bash
# 1. Install from formula
brew install --build-bottle dist/homebrew/topo-gen.rb

# 2. Create bottle
brew bottle topo-gen

# 3. Upload bottle to GitHub releases
# Output: topo-gen--0.22.001.arm64_monterey.bottle.tar.gz

# 4. Update formula with bottle block
```

See `dist/homebrew/BOTTLING.md` for complete bottling instructions.

## Testing Packages

### DMG Testing
```bash
# Mount DMG
open dist/macos-arm64/topo-gen-*.dmg

# Drag app to Applications
# Launch from Applications folder
# Verify app launches without Gatekeeper warnings
```

### PKG Testing
```bash
# Install (requires admin password)
open dist/macos-arm64/topo-gen-*.pkg

# Verify CLI installation
which topo-gen
topo-gen --version

# Verify GUI installation
ls /Applications/Topographic\ Generator.app
```

### Homebrew Testing
```bash
# Test formula locally
brew install --build-from-source dist/homebrew/topo-gen.rb

# Verify installation
brew test topo-gen
topo-gen --version

# Uninstall
brew uninstall topo-gen
```

## Code Signing & Notarization

### Why Sign?

**Without signing**:
- Users see Gatekeeper warnings
- "App is damaged" or "Unidentified developer" messages
- Requires users to bypass security (Control-click > Open)

**With signing**:
- No warnings for users
- Professional appearance
- Required for enterprise distribution

### Requirements

- Apple Developer Program membership ($99/year)
- Developer ID Application certificate
- Developer ID Installer certificate (for PKG)

### Signing Process

#### 1. Get Certificates

```bash
# List available certificates
security find-identity -v -p codesigning

# Expected output:
#  1) ABC123... "Developer ID Application: Your Name (TEAM_ID)"
#  2) DEF456... "Developer ID Installer: Your Name (TEAM_ID)"
```

#### 2. Sign App Bundle (DMG)

```bash
# Sign app
codesign --sign "Developer ID Application: Your Name" \
         --deep \
         --force \
         --options runtime \
         --timestamp \
         build/topo-gen-gui.app

# Verify
codesign --verify --verbose build/topo-gen-gui.app
```

#### 3. Sign PKG

```bash
# PKG signing is automatic with --sign-identity
./scripts/package/create-pkg.sh \
    --sign-identity "Developer ID Installer: Your Name"
```

#### 4. Notarize (Required for macOS 10.15+)

```bash
# Submit for notarization
xcrun notarytool submit topo-gen-0.22.001.dmg \
    --apple-id your@email.com \
    --team-id TEAM_ID \
    --password app-specific-password \
    --wait

# Staple notarization ticket
xcrun stapler staple topo-gen-0.22.001.dmg

# Verify
spctl -a -vvv -t install topo-gen-0.22.001.dmg
```

**Note**: App-specific password is generated at [appleid.apple.com](https://appleid.apple.com)

### Automated Signing

Add to your deployment script:

```bash
SIGN_APP="Developer ID Application: Your Name"
SIGN_PKG="Developer ID Installer: Your Name"

# Sign before creating DMG
codesign --sign "$SIGN_APP" --options runtime build/topo-gen-gui.app

# Create signed PKG
./scripts/package/create-pkg.sh --sign-identity "$SIGN_PKG"
```

## Distribution Workflow

### Complete Release Process

```bash
# 1. Update version
# Edit CMakeLists.txt: project(... VERSION 0.23.0)

# 2. Create all packages
./scripts/deploy.sh --all-macos --clean-build

# 3. (Optional) Sign packages
codesign --sign "Developer ID Application: ..." \
         build/topo-gen-gui.app

# 4. Create versioned release
./scripts/create_version.sh "v0.23.0_release"

# 5. Push Git tag
git push origin v0.23.0

# 6. Create GitHub release
gh release create v0.23.0 \
    dist/source/topo-gen-0.23.0-source-*.tar.gz \
    dist/macos-arm64/topo-gen-0.23.0.dmg \
    dist/macos-arm64/topo-gen-0.23.0.pkg \
    --title "Topographic Generator v0.23.0" \
    --notes "Release notes here"

# 7. Update Homebrew formula with GitHub release URL
# Edit dist/homebrew/topo-gen.rb:
#   url "https://github.com/user/topo-gen/archive/v0.23.0.tar.gz"
#   sha256 "..." (calculate with scripts/package/create-homebrew.sh)

# 8. Push to Homebrew tap or submit PR to core
```

## Troubleshooting

### DMG Creation Fails

**Error**: `hdiutil: attach failed`
- **Solution**: Eject any existing mounted volumes, retry

**Error**: `SetFile: command not found`
- **Solution**: Install Xcode Command Line Tools

**Background doesn't appear**:
- **Solution**: Ensure `background.png` is 600x400px, PNG format
- Check file is in `packaging/macos/dmg-template/`

### PKG Installation Fails

**Error**: "Package is damaged"
- **Solution**: Code sign the PKG with Developer ID Installer

**CLI not in PATH**:
- **Solution**: Restart terminal or source shell profile
- **Manual**: `export PATH="/usr/local/bin:$PATH"`

### Homebrew Formula Issues

**Error**: `sha256 mismatch`
- **Solution**: Recalculate with `shasum -a 256 file.tar.gz`
- Or let create-homebrew.sh calculate automatically

**Build fails**: "Cannot find CGAL"
- **Solution**: Ensure formula declares all dependencies
- Test build: `brew install --build-from-source --verbose`

## Reference

- **Apple Developer**: https://developer.apple.com
- **Code Signing Guide**: https://developer.apple.com/library/archive/technotes/tn2206/
- **Homebrew Documentation**: https://docs.brew.sh
- **Homebrew Formula Cookbook**: https://docs.brew.sh/Formula-Cookbook

## Support

For issues with packaging scripts, create an issue on GitHub or consult:
- `PACKAGING.md` - General packaging guide
- `packaging/common/dependencies.md` - Dependency details
- `packaging/macos/dmg-template/README.md` - DMG customization
