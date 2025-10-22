# Topographic Generator v2.0

**High-Performance Laser-Cuttable Topographic Model Generator**

Generate professional-quality topographic models from elevation data for laser cutting, CNC machining, or 3D printing. This high-performance C++ implementation uses professional-grade geometry libraries (CGAL, GDAL, Eigen) and algorithms adapted from Bambu Slicer to process elevation data into precise, manufacturable layer files.

![Version](https://img.shields.io/badge/version-2.0.0016-blue)
![License](https://img.shields.io/badge/license-MIT-green)
![Platform](https://img.shields.io/badge/platform-macOS%20%7C%20Linux-lightgrey)

---

## Features

### Core Capabilities
- **Automatic Elevation Data**: Downloads and processes SRTM (Shuttle Radar Topography Mission) elevation tiles automatically
- **Multiple Output Formats**: Generate SVG (laser cutting), STL (3D printing), OBJ, and PLY files
- **Layer-Based Design**: Create individual cutting layers or stacked 3D models
- **High Performance**: Parallel processing using Intel TBB and OpenMP for fast generation
- **Robust Geometry**: CGAL-based exact predicates prevent floating-point errors

### Geographic Features
- **OpenStreetMap Integration**: Optionally include roads, buildings, and waterways
- **Coordinate Systems**: Automatic UTM projection with WGS84 input
- **Custom Regions**: Specify any geographic bounding box worldwide

### Processing Options
- **Contour Simplification**: Douglas-Peucker algorithm with configurable tolerance
- **Smoothing**: Chaikin's corner-cutting algorithm for natural contours
- **Feature Filtering**: Remove small features based on area or width
- **Water Bodies**: Fixed-elevation water features

### Manufacturing Optimizations
- **Registration Marks**: Alignment aids for multi-layer assembly
- **Layer Labels**: Automatic elevation labeling with visibility detection
- **Substrate Scaling**: Fit to standard material sizes (mm or inches)
- **Quality Presets**: Draft, medium, high, ultra mesh quality options

---

## Quick Start

### Default Mount Denali Model
```bash
./build/topo-gen
```
Generates a 7-layer Mount Denali model with default settings.

### Create a Configuration File
```bash
./build/topo-gen --create-config my_area.json
```
Creates a template configuration file with all available options.

### Generate from Configuration
```bash
./build/topo-gen --config my_area.json
```

### Custom Area
```bash
./build/topo-gen \
  --upper-left 45.5,-122.7 \
  --lower-right 45.4,-122.6 \
  --num-layers 10 \
  --output-formats svg,stl \
  --base-name portland_hills
```

---

## Building from Source

### Prerequisites

**macOS:**
```bash
# Install Xcode Command Line Tools
xcode-select --install

# Install dependencies via Homebrew
brew install cmake cgal eigen gdal tbb libomp
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get update
sudo apt-get install build-essential cmake \
  libcgal-dev libeigen3-dev libgdal-dev \
  libtbb-dev libcurl4-openssl-dev
```

### Build Commands

**Quick incremental build:**
```bash
./scripts/quick-build.sh
```

**Full rebuild:**
```bash
./scripts/build_macos.sh  # macOS
# or
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

The compiled executable will be at `build/topo-gen`.

---

## Usage

### Command Line Options

Run `./build/topo-gen --help` for complete option documentation. Key options include:

#### Geographic Bounds
- `--upper-left LAT,LON` - Northwest corner coordinates
- `--lower-right LAT,LON` - Southeast corner coordinates

#### Layer Configuration
- `--num-layers N` - Number of cutting layers (default: 7)
- `--height-per-layer M` - Elevation step between layers (meters)
- `--layer-thickness-mm MM` - Material thickness (default: 3.0)

#### Output Control
- `--output-formats svg,stl,obj,ply` - Output file formats (default: svg)
- `--output-layers` - Generate individual layer files (default: true)
- `--output-stacked` - Generate single 3D stacked model
- `--base-name NAME` - Output filename prefix

#### Processing
- `--simplify-tolerance N` - Contour simplification (default: 5.0)
- `--smoothing N` - Smoothing iterations (default: 1)
- `--min-area M` - Remove features smaller than M square meters
- `--min-feature-width-mm MM` - Minimum feature width (default: 2.0)

#### Features
- `--include-roads` - Add OpenStreetMap roads
- `--include-buildings` - Add OSM building footprints
- `--include-waterways` - Add OSM waterways

#### Manufacturing
- `--substrate-size-mm MM` - Scale to fit substrate (default: 200)
- `--add-registration-marks` - Include alignment marks
- `--include-layer-numbers` - Label layers with elevations

#### Advanced
- `--quality draft|medium|high|ultra` - Mesh quality preset
- `--color-scheme terrain|rainbow|topographic` - Coloring scheme
- `--verbose` - Enable detailed logging
- `--log-file PATH` - Write log to file

### Configuration File Format

Configuration files use JSON format. Generate a template with `--create-config`:

```json
{
  "upper_left_lat": 63.1497,
  "upper_left_lon": -151.1847,
  "lower_right_lat": 62.9887,
  "lower_right_lon": -150.8293,
  "num_layers": 7,
  "height_per_layer": 21.43,
  "base_name": "mount_denali",
  "output_formats": "svg",
  "simplify_tolerance": 5.0,
  "smoothing": 1,
  "substrate_size_mm": 200.0
}
```

See `doc/example_config_annotated.json` for all available options with explanations.

---

## Output Files

### SVG Files (Laser Cutting)
- Individual layer files: `{base_name}_layer_{N}.svg`
- Stacked model: `{base_name}_stacked.svg`
- Units: millimeters
- Includes: Cut paths, registration marks, labels

### STL Files (3D Printing)
- Individual layers: `{base_name}_layer_{N}.stl`
- Stacked model: `{base_name}_stacked.stl`
- Binary STL format
- Units: millimeters

### OBJ/PLY Files
- Full 3D mesh representations
- Include elevation-based coloring (if enabled)
- Suitable for visualization or further processing

---

## Dependencies

### Required Libraries
- **CGAL** ≥5.6 - Computational Geometry Algorithms Library
- **GDAL** ≥3.0 - Geospatial Data Abstraction Library
- **Eigen3** ≥3.3 - Linear algebra library
- **Intel TBB** ≥2021 - Threading Building Blocks for parallelism
- **libcurl** - HTTP client for SRTM downloads
- **nlohmann/json** - JSON parsing (header-only, included)

### Optional Libraries
- **OpenMP** - Additional parallel acceleration
- **libigl** - Advanced mesh processing algorithms

### Build Tools
- **CMake** ≥3.20
- **C++20** compatible compiler (GCC 10+, Clang 12+, Apple Clang 13+)

---

## Project Structure

```
cpp-version/
├── build/              # Build artifacts (created by CMake)
├── doc/                # Documentation files
│   ├── COPYRIGHT       # License and attribution
│   ├── LICENSE         # MIT License text
│   └── ACKNOWLEDGMENTS.md  # Third-party acknowledgments
├── include/            # Public header files
│   └── topographic_generator.hpp
├── scripts/            # Build and deployment scripts
│   ├── quick-build.sh
│   ├── build_macos.sh
│   └── deploy.sh
├── src/                # Source code
│   ├── cli/            # Command-line interface
│   ├── core/           # Core processing logic
│   ├── export/         # Output format exporters
│   └── main.cpp        # Entry point
├── test/               # Test files and outputs
├── CMakeLists.txt      # CMake configuration
├── CLAUDE.md           # AI assistant guidance
└── README.md           # This file
```

---

## Testing

Run comprehensive tests:
```bash
# Quick test with 2 layers
cd build
./topo-gen --num-layers 2 --output-formats svg --base-name test

# Verbose logging for debugging
./topo-gen --verbose --log-file generation.log
```

All test outputs should be placed in `test/output/` directory.

---

## Performance

### Typical Performance (Apple M1 Pro, 8 cores)
- **Small area** (10km × 10km, 7 layers): ~5-15 seconds
- **Medium area** (30km × 30km, 10 layers): ~30-60 seconds
- **Large area** (60km × 60km, 15 layers): ~2-5 minutes

Performance scales well with:
- Number of CPU cores (parallel processing)
- Available RAM (mesh size)
- SSD vs HDD (elevation data caching)

---

## Troubleshooting

### "Invalid latitude" errors
The system now automatically detects coordinate systems. If you see this error with pre-projected data, please report it as a bug.

### Memory issues
For very large areas, consider:
- Reducing `num-layers`
- Increasing `simplify-tolerance`
- Using `--quality draft`

### Missing elevation data
SRTM data is automatically downloaded. Ensure internet connectivity for first run. Data is cached in the system's temporary directory.

### Build failures
- Verify all dependencies are installed: `cmake .. --debug-find`
- Check compiler version: `c++ --version` (must support C++20)
- Try clean rebuild: `rm -rf build && ./scripts/build_macos.sh`

---

## License

This project is licensed under the **MIT License** - see the [LICENSE](LICENSE) file for details.

**Copyright © 2025 Matthew Block. All rights reserved.**

### Third-Party Software

This software incorporates or adapts code from:
- **laser_slicer** by Boris Legradic (MIT License) - Core topographic algorithms
- **Bambu Slicer** (libslic3r) (AGPL-3.0) - Triangle-plane intersection algorithms

And uses these third-party libraries:
- CGAL (GPL/LGPL), GDAL (MIT/X), Eigen3 (MPL 2.0), Intel TBB (Apache 2.0), libigl (MPL 2.0/GPL)

See [COPYRIGHT](COPYRIGHT) and [doc/ACKNOWLEDGMENTS.md](doc/ACKNOWLEDGMENTS.md) for complete attribution.

---

## Development

### Contributing
This project was developed with significant assistance from **Claude** (Anthropic AI Assistant), including architectural design, algorithm implementation, and optimization strategies.

### Version History
- **v2.0016 (Version 13)** - Production release
  - Removed Python version dependencies
  - Enhanced coordinate system detection
  - Improved help output and documentation
  - Performance optimizations

### Future Enhancements
- Flexible unit system (meters/feet/miles, DMS coordinates)
- Comprehensive test suite with validation
- Additional output format options

---

## Support

For questions, issues, or feature requests:
- **GitHub Issues**: Report bugs and request features
- **Email**: Matthew Block (see COPYRIGHT file)
- **Documentation**: See `doc/` directory for detailed guides

---

## Acknowledgments

Developed by **Matthew Block** with architectural design and implementation assistance from **Claude** (Anthropic AI Assistant).

Core algorithms adapted from:
- Boris Legradic's **laser_slicer** project
- Bambu Lab's **Bambu Slicer** (libslic3r) geometry processing

Built with professional-grade open source libraries: CGAL, GDAL, Eigen, Intel TBB, and many others. See [doc/ACKNOWLEDGMENTS.md](doc/ACKNOWLEDGMENTS.md) for complete attribution.

---

**Generate your topographic masterpiece today!** 🏔️
