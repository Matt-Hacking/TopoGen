# Linux Packaging Guide

Complete guide for creating Linux distribution packages (DEB, RPM, Flatpak, AppImage).

## Prerequisites

- Linux system (or cross-compilation setup)
- Built executables in `build/`
- Bundled dependencies (shared libraries)
- Package-specific tools (dpkg-deb, rpmbuild, flatpak-builder, appimagetool)

## Quick Start

### Create All Linux Packages

```bash
# Build everything and create all Linux packages
./scripts/deploy.sh --all-linux --clean-build
```

This creates:
- Source tarball (`dist/source/`)
- Binary package (`dist/linux-x64/`)
- DEB package (`dist/linux-x64/`)
- RPM package (`dist/linux-x64/`)
- Flatpak manifest (`dist/flatpak/`)
- AppImage (`dist/linux-x64/`)

### Individual Package Types

#### DEB Package (Debian/Ubuntu)

```bash
./scripts/deploy.sh --deb

# Or directly
./scripts/package/create-deb.sh \
    --cli build/topo-gen \
    --gui build/topo-gen-gui \
    --deps-dir dist/linux-deps
```

**Output**: `dist/linux-x64/topo-gen_{version}_amd64.deb`

**Use case**: Debian, Ubuntu, Linux Mint, elementary OS

**Install**: `sudo dpkg -i topo-gen_*.deb && sudo apt-get install -f`

#### RPM Package (Fedora/RHEL)

```bash
./scripts/deploy.sh --rpm

# Or directly
./scripts/package/create-rpm.sh \
    --cli build/topo-gen \
    --gui build/topo-gen-gui \
    --deps-dir dist/linux-deps
```

**Output**: `dist/linux-x64/topo-gen-{version}-1.x86_64.rpm`

**Use case**: Fedora, RHEL, CentOS, openSUSE

**Install**: `sudo dnf install topo-gen-*.rpm` or `sudo rpm -i topo-gen-*.rpm`

#### Flatpak (Universal)

```bash
./scripts/deploy.sh --flatpak

# Or directly
./scripts/package/create-flatpak.sh \
    --source-url https://github.com/user/topo-gen/archive/v0.22.001.tar.gz
```

**Output**: `dist/flatpak/com.matthewblock.TopoGen.json`

**Use case**: All Linux distributions via Flathub

**Build**: `flatpak-builder build-dir dist/flatpak/*.json`

#### AppImage (Portable)

```bash
./scripts/deploy.sh --appimage

# Or directly
./scripts/package/create-appimage.sh \
    --gui build/topo-gen-gui \
    --deps-dir dist/linux-deps
```

**Output**: `dist/linux-x64/TopoGen-{version}-x86_64.AppImage`

**Use case**: Portable single-file executable, all Linux distributions

**Run**: `chmod +x TopoGen-*.AppImage && ./TopoGen-*.AppImage`

## Package Formats Overview

| Format | Target | Install | Pros | Cons |
|--------|--------|---------|------|------|
| **DEB** | Debian-based | System-wide | Native integration, dependency management | Distro-specific |
| **RPM** | Fedora-based | System-wide | Native integration, dependency management | Distro-specific |
| **Flatpak** | Universal | Sandboxed | Works everywhere, automatic updates | Larger size, sandboxing |
| **AppImage** | Universal | Portable | No install, single file | No auto-updates, larger size |

## DEB Package Details

### Installation Locations
- CLI: `/usr/local/bin/topo-gen`
- GUI: `/opt/topo-gen/topo-gen-gui` (wrapper at `/usr/local/bin/topo-gen-gui`)
- Libraries: `/opt/topo-gen/lib/`
- Data: `/opt/topo-gen/share/gdal/`
- Desktop: `/usr/share/applications/topo-gen.desktop`

### Dependencies
```
libc6 (>= 2.31), libstdc++6 (>= 10), libgcc-s1,
libgdal30 | libgdal28, libtbb12 | libtbb2
```

### Post-Install
- Updates desktop database
- Updates icon cache
- Creates environment file at `/etc/profile.d/topo-gen.sh`
- Sets `GDAL_DATA` environment variable

### Testing
```bash
sudo dpkg -i topo-gen_*.deb
sudo apt-get install -f  # Install dependencies
topo-gen --version
topo-gen-gui
sudo apt-get remove topo-gen  # Uninstall
```

## RPM Package Details

### Installation Locations
Same as DEB package

### Dependencies
```
glibc >= 2.31, libstdc++ >= 10, gdal-libs, tbb
```

### Post-Install
- Updates desktop database
- Updates icon cache
- Creates environment file at `/etc/profile.d/topo-gen.sh`

### Testing
```bash
sudo rpm -i topo-gen-*.rpm
# Or: sudo dnf install topo-gen-*.rpm
topo-gen --version
topo-gen-gui
sudo rpm -e topo-gen  # Uninstall
```

## Flatpak Details

### Manifest Structure
```json
{
  "app-id": "com.matthewblock.TopoGen",
  "runtime": "org.freedesktop.Platform",
  "runtime-version": "23.08",
  "sdk": "org.freedesktop.Sdk",
  "command": "topo-gen-gui",
  "finish-args": ["--share=ipc", "--socket=x11", ...],
  "modules": [
    {"name": "gdal", ...},
    {"name": "cgal", ...},
    {"name": "topo-gen", ...}
  ]
}
```

### Local Testing
```bash
# Install flatpak-builder
sudo apt install flatpak-builder  # Debian/Ubuntu
sudo dnf install flatpak-builder  # Fedora

# Add Flathub
flatpak remote-add --if-not-exists flathub https://flathub.org/repo/flathub.flatpakrepo

# Build
flatpak-builder --force-clean build-dir dist/flatpak/*.json

# Test
flatpak-builder --run build-dir dist/flatpak/*.json topo-gen-gui

# Install locally
flatpak-builder --user --install build-dir dist/flatpak/*.json
flatpak run com.matthewblock.TopoGen
```

### Flathub Submission
1. Fork https://github.com/flathub/flathub
2. Create repository for app
3. Add manifest
4. Test locally
5. Submit pull request
6. Follow review process

See: https://docs.flathub.org/docs/for-app-authors/submission/

## AppImage Details

### Structure
AppImages are self-contained with:
- AppDir/ containing usr/bin/, usr/lib/, usr/share/
- AppRun script setting LD_LIBRARY_PATH
- Desktop file and icon
- All bundled dependencies

### AppRun Script
```bash
#!/bin/bash
APPDIR="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$APPDIR/usr/lib:$LD_LIBRARY_PATH"
export GDAL_DATA="$APPDIR/usr/share/gdal"
exec "$APPDIR/usr/bin/topo-gen-gui" "$@"
```

### Testing
```bash
chmod +x TopoGen-*.AppImage
./TopoGen-*.AppImage
```

### Distribution
- Upload to GitHub releases
- Users download and run
- No installation required
- Works on most Linux distributions

## Dependency Bundling

All Linux packages require bundled libraries. Use Phase 1 bundler:

```bash
./packaging/linux/bundle_deps.sh \
    --executable build/topo-gen \
    --output dist/linux-deps
```

This bundles:
- GDAL libraries
- CGAL/GMP dependencies
- TBB libraries
- Qt6 libraries (for GUI)
- Standard C++ libraries

## Distribution Workflow

### Complete Release Process

```bash
# 1. Update version in CMakeLists.txt
# 2. Build executables
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# 3. Bundle dependencies
./packaging/linux/bundle_deps.sh \
    --executable build/topo-gen \
    --output dist/linux-deps

# 4. Create all Linux packages
./scripts/deploy.sh --all-linux --clean-build

# 5. Test packages
sudo dpkg -i dist/linux-x64/*.deb
sudo rpm -i dist/linux-x64/*.rpm
./dist/linux-x64/TopoGen-*.AppImage

# 6. Create GitHub release
gh release create v0.23.0 \
    dist/source/topo-gen-*.tar.gz \
    dist/linux-x64/*.deb \
    dist/linux-x64/*.rpm \
    dist/linux-x64/TopoGen-*.AppImage \
    --title "v0.23.0"

# 7. Submit Flatpak to Flathub (separate process)
```

## Troubleshooting

### DEB Issues
**Error**: "dpkg: dependency problems"
- Solution: `sudo apt-get install -f`

**Error**: "Package has unmet dependencies"
- Solution: Check dependencies are available in repositories

### RPM Issues
**Error**: "Failed dependencies"
- Solution: `sudo dnf install topo-gen-*.rpm` (auto-resolves deps)

**Error**: "rpmbuild not found"
- Solution: `sudo dnf install rpm-build`

### Flatpak Issues
**Error**: "Runtime not found"
- Solution: `flatpak install flathub org.freedesktop.Platform//23.08`

**Error**: "Build failed"
- Solution: Check manifest syntax, ensure source URL accessible

### AppImage Issues
**Error**: "appimagetool not found"
- Solution: Script downloads automatically, or get from AppImageKit releases

**Error**: "FUSE not available"
- Solution: Extract and run: `./TopoGen-*.AppImage --appimage-extract && ./squashfs-root/AppRun`

## Reference

- **Debian Packaging**: https://www.debian.org/doc/manuals/maint-guide/
- **RPM Packaging**: https://rpm-packaging-guide.github.io/
- **Flatpak Documentation**: https://docs.flatpak.org/
- **AppImage Documentation**: https://docs.appimage.org/

## Support

For packaging issues:
- `PACKAGING.md` - General packaging guide
- `packaging/common/dependencies.md` - Dependency details
- `packaging/linux/bundle_deps.sh` - Linux dependency bundler (Phase 1)
