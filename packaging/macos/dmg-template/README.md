# DMG Template Resources

This directory contains resources for creating macOS DMG installers.

## Files

### background.png (Optional)
Custom background image for the DMG window.

**Specifications:**
- Dimensions: 600x400 pixels (matches default window size)
- Format: PNG with transparency
- Design: Should include visual cue (arrow) pointing from app icon to Applications folder

**Recommended Design:**
```
+----------------------------------------------------------+
|                                                          |
|                                                          |
|         [App Icon]  ------>  [Applications Icon]        |
|         (150, 200)            (450, 200)                 |
|                                                          |
|    "Drag to install"                                     |
|                                                          |
+----------------------------------------------------------+
```

**Creating the Background:**

1. **Using GraphicsWizard or Photoshop:**
   ```
   - Create 600x400px image
   - Add subtle gradient or solid background
   - Draw arrow from left (150, 200) to right (450, 200)
   - Add text: "Drag Topographic Generator to Applications"
   - Export as background.png
   ```

2. **Using Sketch/Figma:**
   ```
   - Artboard: 600x400px
   - Add arrow graphic
   - Add instructional text
   - Export as PNG
   ```

3. **Using ImageMagick (Command Line):**
   ```bash
   convert -size 600x400 xc:white \
           -font Arial -pointsize 20 \
           -draw "text 180,350 'Drag to Applications to install'" \
           -draw "line 200,200 400,200" \
           -draw "polygon 400,190 420,200 400,210" \
           background.png
   ```

### VolumeIcon.icns (Optional)
Custom volume icon that appears when DMG is mounted.

**Creating:**
```bash
# From PNG (512x512 recommended)
mkdir icon.iconset
sips -z 16 16     icon-1024.png --out icon.iconset/icon_16x16.png
sips -z 32 32     icon-1024.png --out icon.iconset/icon_16x16@2x.png
sips -z 32 32     icon-1024.png --out icon.iconset/icon_32x32.png
sips -z 64 64     icon-1024.png --out icon.iconset/icon_32x32@2x.png
sips -z 128 128   icon-1024.png --out icon.iconset/icon_128x128.png
sips -z 256 256   icon-1024.png --out icon.iconset/icon_128x128@2x.png
sips -z 256 256   icon-1024.png --out icon.iconset/icon_256x256.png
sips -z 512 512   icon-1024.png --out icon.iconset/icon_256x256@2x.png
sips -z 512 512   icon-1024.png --out icon.iconset/icon_512x512.png
cp icon-1024.png icon.iconset/icon_512x512@2x.png
iconutil -c icns icon.iconset -o VolumeIcon.icns
```

## Fallback Behavior

If these files are not present, the `create-dmg.sh` script will:
- Use the default macOS DMG appearance (no custom background)
- Use standard Finder icons
- Still create a functional DMG with proper drag-to-Applications layout

## Testing Your Custom Resources

After creating custom resources:

```bash
# Create DMG with custom resources
./scripts/package/create-dmg.sh build/topo-gen-gui.app

# Mount and verify
open dist/macos-arm64/topo-gen-*.dmg
```

The custom background and icon should appear when the DMG is mounted.

## Tips

- **Keep it Simple**: Users just need to know "drag left icon to right"
- **Use Brand Colors**: Match your app's color scheme
- **Test on Different Macs**: Retina and non-retina displays
- **Accessibility**: Ensure text is readable, use sufficient contrast
- **File Size**: Keep background image < 500KB

## Example Projects

Look at popular macOS apps for inspiration:
- VLC media player
- Firefox
- Blender
- Docker Desktop

All use simple, clear drag-to-install designs.
