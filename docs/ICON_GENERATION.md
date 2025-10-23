# App Icon Generation

This document describes the process for generating application icons for the Topographic Generator GUI across all supported platforms (macOS, Windows, Linux).

## Overview

The app icon is a 3D visualization of Denali (Mount McKinley) created using the topographic generator itself. This "dogfooding" approach creates an authentic, recognizable icon that showcases the product's capabilities.

## Design Concept

- **Subject**: Denali, Alaska (63.069°N, 151.007°W)
- **Layers**: 11 total, keeping only odd-numbered layers (1, 3, 5, 7, 9, 11)
- **Orientation**: 30° tilt forward, 30° tilt left
- **Lighting**: From right side to create depth through shadows
- **Style**: 3D visualization with gradient coloring (blue at base → white at peak)

## Requirements

### Software Dependencies

**Python packages:**
```bash
pip install numpy trimesh svgwrite pillow
```

**Optional (for better SVG rasterization):**
```bash
pip install cairosvg
```

**Platform-specific tools:**
- **macOS**: `iconutil` (included with Xcode Command Line Tools)
- **Windows**: No additional tools required (uses PIL)
- **Linux**: No additional tools required (uses PIL)

## Generation Process

### Step 1: Generate Denali STL Layers

First, use topo-gen to generate 11 STL layers of Denali:

```bash
cd cpp-version
./build/topo-gen \
    --upper-left 63.15,-151.1 \
    --lower-right 63.0,-150.9 \
    --num-layers 11 \
    --force-all-layers \
    --output-format stl \
    --output-base-name denali_icon \
    --output-dir /tmp/denali_layers
```

This creates STL files:
- `/tmp/denali_layers/denali_icon-01.stl`
- `/tmp/denali_layers/denali_icon-02.stl`
- ...
- `/tmp/denali_layers/denali_icon-11.stl`

### Step 2: Generate Icon Files

Run the icon generation script:

```bash
cd cpp-version
./scripts/generate_app_icon.py /tmp/denali_layers \
    --output-dir src/gui/resources \
    --layers 1 3 5 7 9 11 \
    --tilt-forward 30 \
    --tilt-left 30 \
    --size 1024
```

This generates:
- `src/gui/resources/icons/app_icon.svg` - Master scalable icon (1024x1024)
- `src/gui/resources/icons/app_icon.icns` - macOS icon bundle
- `src/gui/resources/icons/app_icon.ico` - Windows icon
- `src/gui/resources/icons/hicolor/{size}/apps/topo-gen-gui.png` - Linux PNG icons (16, 32, 48, 64, 128, 256, 512)

### Step 3: Rebuild Application

The CMake build system will automatically incorporate the icons:

```bash
cd cpp-version
./scripts/quick-build.sh
```

## Platform Integration

### macOS

Icons are embedded in the app bundle:
- **Location**: `topo-gen-gui.app/Contents/Resources/app_icon.icns`
- **CMake property**: `MACOSX_BUNDLE_ICON_FILE`
- **Resolutions**: 16x16 to 1024x1024 (including @2x retina variants)

### Windows

Icons are compiled into the executable as a resource:
- **Resource file**: `src/gui/resources/app.rc` (generated from `app.rc.in`)
- **Icon ID**: `IDI_ICON1`
- **Resolutions**: 16x16 to 256x256

### Linux

Icons are installed system-wide following XDG specifications:
- **Desktop file**: `/usr/share/applications/topo-gen-gui.desktop`
- **Scalable**: `/usr/share/icons/hicolor/scalable/apps/topo-gen-gui.svg`
- **PNG icons**: `/usr/share/icons/hicolor/{size}x{size}/apps/topo-gen-gui.png`

## Customization

### Adjusting the View

You can modify the viewing angle by changing the tilt parameters:

```bash
./scripts/generate_app_icon.py /tmp/denali_layers \
    --tilt-forward 45 \    # More dramatic forward tilt
    --tilt-left 20 \       # Less left rotation
    --size 2048           # Higher resolution master
```

### Using Different Layers

To change which layers are visible:

```bash
./scripts/generate_app_icon.py /tmp/denali_layers \
    --layers 2 4 6 8 10    # Use even layers instead
```

### Different Location

To create an icon of a different mountain or location:

1. Generate STL layers for your chosen location
2. Run the icon generation script with the new STL directory
3. Adjust layer numbers and tilt angles as needed

## Troubleshooting

### iconutil not found (macOS)

Install Xcode Command Line Tools:
```bash
xcode-select --install
```

### SVG rasterization quality

For best quality PNG generation, install cairosvg:
```bash
pip install cairosvg
```

Without cairosvg, the script uses PIL fallback which creates simpler rasterization.

### Icon not appearing in built app

1. **Verify files exist**: Check that icon files are in `src/gui/resources/icons/`
2. **Rebuild completely**: `./scripts/clean.sh && ./scripts/quick-build.sh`
3. **macOS**: Check Info.plist in app bundle
4. **Windows**: Verify app.rc was compiled into executable
5. **Linux**: Run `update-desktop-database` after installation

### Layer files not found

Ensure STL files follow expected naming patterns:
- `*-01.stl`, `*-02.stl`, etc.
- `layer_01*.stl`, `layer_02*.stl`, etc.

The script tries multiple patterns to find layer files.

## Future Enhancements

Potential improvements to the icon system:

1. **Animated icon**: Rotating 3D visualization (for platforms that support it)
2. **Theme variants**: Light and dark mode icons
3. **Alternative designs**: Different mountains, different layer counts
4. **Real-time rendering**: Generate icon directly from 3D mesh without STL intermediate
5. **GUI tool**: Interactive icon generator with live preview

## Credits

Icon generation system created using:
- **trimesh**: 3D mesh loading and manipulation
- **svgwrite**: SVG generation
- **Pillow (PIL)**: Image processing and format conversion
- **numpy**: Mathematical operations

Icon design concept by Matthew Block.

## License

The icon generation scripts and icon assets are licensed under the MIT License.
Copyright (c) 2025 Matthew Block.
