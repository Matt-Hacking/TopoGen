# Topographic Model Labeling Guide

## Overview

The topographic model generator supports comprehensive 2D labeling for laser-cut layers. Labels can include scale information, elevation data, geographic coordinates, layer numbers, and custom text. The labeling system uses pattern substitution and adaptive text fitting to ensure labels fit within available space.

## Label Types

### Visible Labels

Visible labels are placed in the corners of each layer and remain visible after assembly. These are ideal for:
- Layer identification numbers
- Scale information
- Project titles
- Assembly instructions

**Placement**: Lower-left corner (base layer) or lower-right corner (regular layers)

### Hidden Labels

Hidden labels are placed in the center of each layer and are covered by the next layer when stacked. These are useful for:
- Internal layer identification
- Quality control information
- Manufacturing metadata
- Geographic coordinates

**Placement**: Center of the layer (within the area that will be covered by the next layer)

**Important**: Hidden labels are positioned to ensure they will be covered after assembly. They are placed in the center 60% of the layer area, leaving a 20% margin on all sides.

## Pattern Substitution

Label templates support pattern substitution to automatically insert dynamic information.

**New Syntax**: Patterns use `%{identifier}` format for unambiguous matching.

| Pattern | Description | Example Output |
|---------|-------------|----------------|
| `%{s}` | Map scale ratio | `25000` (for 1:25000 scale) |
| `%{c}` | Contour height/interval | `10.0m` or `32.8ft` |
| `%{n}` | Layer number | `1`, `2`, `3`, ... |
| `%{x}` | Center longitude | `-122.3321°` |
| `%{y}` | Center latitude | `47.6062°` |
| `%{w}` | Geographic area width | `1500.0m` or `4921.3ft` |
| `%{h}` | Geographic area height | `1200.0m` or `3937.0ft` |
| `%{W}` | Substrate width | `300.0mm` or `11.8in` |
| `%{H}` | Substrate height | `200.0mm` or `7.9in` |
| `%{C}` | Center coordinates (full) | `47.6062°, -122.3321°` |
| `%{UL}` | Upper-left coordinates | `47.6100°, -122.3400°` |
| `%{UR}` | Upper-right coordinates | `47.6100°, -122.3200°` |
| `%{LL}` | Lower-left coordinates | `47.6000°, -122.3400°` |
| `%{LR}` | Lower-right coordinates | `47.6000°, -122.3200°` |

### Escaping Patterns

To include literal pattern text in your labels, use double-percent escaping:

```
%%{s}  → Renders as "%{s}" (not substituted)
%%{c}  → Renders as "%{c}" (not substituted)
```

**Example**: `"Scale %%{s} means 1:%{s}"` renders as `"Scale %{s} means 1:25000"`

### Coordinate Formatting

- **Metric units**: Decimal degrees (e.g., `47.6062°, -122.3321°`)
- **Imperial units**: Degrees, minutes, seconds (e.g., `47°36'22.3"N, 122°19'55.6"W`)

### Distance Formatting

- **Land units** (elevations, contour heights): Meters or feet
- **Print units** (substrate dimensions): Millimeters or inches

## Usage Examples

### Command Line

```bash
# Basic visible label on base layer
topo-gen --base-label-visible "Mount Denali 1:25000"

# Scale and contour height with pattern substitution
topo-gen --base-label-visible "Scale 1:%{s}, Contour %{c}"

# Layer numbers on each layer
topo-gen --layer-label-visible "Layer %{n}"

# Hidden labels with coordinates
topo-gen --base-label-hidden "Center: %{C}"

# Complete labeling setup
topo-gen \
  --base-label-visible "Scale 1:%{s}, Contour %{c}" \
  --base-label-hidden "Center: %{C}, Area: %{w} × %{h}" \
  --layer-label-visible "Layer %{n} of 10" \
  --layer-label-hidden "Elevation: %{c}, Substrate: %{W} × %{H}"
```

### Configuration File

```json
{
  "base_label_visible": "Scale 1:%{s}, Contour %{c}",
  "base_label_hidden": "Center: %{C}",
  "layer_label_visible": "Layer %{n}",
  "layer_label_hidden": "Elevation: %{c}",
  "visible_label_color": "#000000",
  "hidden_label_color": "#666666",
  "base_font_size_mm": 4.0,
  "layer_font_size_mm": 3.0,
  "label_units": "metric",
  "print_units": "mm",
  "land_units": "meters"
}
```

## Adaptive Text Fitting

The labeling system uses a four-stage adaptive fitting algorithm to ensure labels fit within available space:

### Stage 1: Bending (0° to 15°)

Text is curved along a path to fit horizontally constrained spaces. The system tries progressively larger bend angles until the text fits or the maximum bend angle (15°) is reached.

**When used**: Text is too wide but close to fitting
**Result**: Text follows a slight arc to save horizontal space

### Stage 2: Scaling (100% to 50%)

Font size is reduced to fit the text within the available area. The system tries progressively smaller sizes down to a minimum of 50% of the original size.

**When used**: Bending is insufficient
**Result**: Smaller but still legible text (minimum 1.5mm)

### Stage 3: Splitting (2 or 3 parts)

Text is split into multiple lines that are vertically stacked. Each part is positioned independently.

**When used**: Scaling is insufficient
**Result**: Multi-line text with controlled line spacing

### Stage 4: Truncation

As a last resort, text is truncated with an ellipsis (…) to fit the available width.

**When used**: All other methods fail
**Result**: Partial text with ellipsis indicator

### Fitting Warnings

When text requires modification to fit, the system logs warnings (visible with `--verbose` or log level ≥ 4):

```
Visible label: Text scaled to 75% of original size to fit
Hidden label: Text split into 2 parts to fit available space
```

## Label Styling

### Colors

- **Visible labels**: Black (#000000) by default, customizable via `visible_label_color`
- **Hidden labels**: Gray (#666666) by default, customizable via `hidden_label_color`, rendered at 70% opacity in SVG

### Font Sizes

- **Base layer labels**: 4.0mm by default (`base_font_size_mm`)
- **Regular layer labels**: 3.0mm by default (`layer_font_size_mm`)

Sizes are automatically converted for PNG export using the DPI setting (default 600 DPI):
- 4.0mm @ 600 DPI = 94 pixels
- 3.0mm @ 600 DPI = 71 pixels

### Font Family

All labels use Arial sans-serif font for maximum compatibility and legibility at small sizes.

## Format Support

### SVG (Fully Supported)

SVG labels are rendered as `<text>` elements with:
- Precise positioning and alignment
- Font size in millimeters
- Support for all text fitting stages (bend, scale, split, truncate)
- Separate groups for visible and hidden labels
- Inkscape-compatible layer labels

### PNG (Stub Implementation)

PNG text rendering is currently a stub that draws placeholder rectangles where text would appear. A warning is logged on first use:

```
WARNING: PNG text rendering not yet implemented.
         Text labels will be rendered as placeholder rectangles.
         To add full text support, integrate FreeType library.
```

**Future Enhancement**: Full PNG text rendering requires integrating the FreeType library for pixel-level text rasterization.

## Best Practices

### 1. Keep Labels Concise

Shorter labels are less likely to require adaptive fitting. Aim for:
- Visible labels: 20-30 characters
- Hidden labels: 30-50 characters

### 2. Test with Verbose Logging

Use `--verbose` or `--log-level 4` to see fitting warnings and verify labels are rendering as expected:

```bash
topo-gen --base-label-visible "Your Label Here" --verbose
```

### 3. Consider Assembly Order

Remember that hidden labels will be covered by the next layer:
- Base layer hidden label is covered by layer 1
- Layer 1 hidden label is covered by layer 2
- Top layer has no hidden label (nothing to cover it)

### 4. Match Units to Your Workflow

Set units appropriate for your use case:
- **Metric**: `--label-units metric --print-units mm --land-units meters`
- **Imperial**: `--label-units imperial --print-units inches --land-units feet`

### 5. Preview Labels Before Cutting

Always preview generated SVG files in Inkscape or a browser before laser cutting to verify label placement and content.

## Troubleshooting

### Labels Not Appearing

**Problem**: Labels are not visible in SVG output
**Solution**: Ensure label templates are not empty strings. Check that at least one label option is set:

```bash
topo-gen --base-label-visible "Test Label"
```

### Labels Too Small/Large

**Problem**: Label text is too small or too large
**Solution**: Adjust font sizes:

```bash
topo-gen \
  --base-label-visible "Test" \
  --base-font-size-mm 5.0 \
  --layer-font-size-mm 4.0
```

### Hidden Labels Visible After Assembly

**Problem**: Hidden labels can be seen after stacking layers
**Solution**: This indicates the hidden area calculation is incorrect. Verify that:
1. All layers are the same size
2. Layers are properly aligned during assembly
3. The substrate size is correctly specified

### Pattern Not Substituted

**Problem**: Pattern like `%{s}` appears literally in output
**Solution**: Ensure the pattern is spelled correctly and that the required data is available:
- `%{s}` requires scale ratio to be computed from scale factor
- `%{c}` requires contour interval to be set
- `%{n}` requires layer number (won't substitute on base layer if layer_number = 0)
- Coordinate patterns require valid geographic bounds
- Unknown patterns are kept as-is (e.g., `%{foo}` → `%{foo}`)

### Text Truncation Warnings

**Problem**: Text is being truncated unexpectedly
**Solution**:
1. Shorten label text
2. Increase font size minimum (`min_legible_size_mm`)
3. Increase substrate size to provide more space

## Implementation Details

### Classes

- **LabelRenderer**: Pattern substitution and label placement logic
- **TextFitter**: Adaptive text fitting algorithm (bend/scale/split/truncate)
- **SVGExporter**: SVG text element generation
- **RasterAnnotator**: PNG text rendering (stub implementation)

### Configuration Flow

```
TopographicConfig (user input)
    ↓
SVGConfig (SVG-specific settings)
    ↓
LabelRenderer + TextFitter
    ↓
SVG <text> elements
```

### Coordinate Systems

Labels use the same coordinate system as the layer content:
- Origin: Top-left corner of SVG canvas
- X-axis: Left to right
- Y-axis: Top to bottom
- Units: Millimeters

### Bounding Boxes

Two bounding boxes control label placement:
- **Content bbox**: Full printable area (within margins)
- **Hidden bbox**: Center 60% of content area (safe zone for hidden labels)

## Advanced Usage

### Custom Label Positioning (Future Enhancement)

Currently, labels are automatically positioned:
- Visible: Corners (base=lower-left, layers=lower-right)
- Hidden: Center of hidden area

Future versions may support custom positioning via:
```json
{
  "base_label_visible": "Text",
  "base_label_visible_position": {"x": 50, "y": 50, "anchor": "middle"}
}
```

### Multi-Line Labels (Future Enhancement)

Currently, multi-line labels require splitting via the adaptive fitting algorithm. Future versions may support explicit line breaks:
```bash
--base-label-visible "Line 1\nLine 2\nLine 3"
```

### Label Rotation (Future Enhancement)

Future versions may support rotated labels:
```json
{
  "base_label_visible": "Vertical Text",
  "base_label_visible_rotation": 90
}
```

## See Also

- [Pattern Substitution Reference](pattern_substitution.md) (TODO)
- [SVG Export Guide](svg_export.md) (TODO)
- [Configuration File Reference](configuration.md) (TODO)
