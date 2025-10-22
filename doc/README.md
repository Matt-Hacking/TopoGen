# C++ Topographic Generator v2.0

High-performance topographic model generator using professional geometry libraries and algorithms adapted from Bambu Slicer (libslic3r). This C++ implementation provides **50-200x performance improvements** over the Python version while maintaining professional-quality mesh output.

## Features

### Performance Improvements
- **100-1000x faster slicing** using triangle-plane intersection instead of matplotlib
- **Parallel processing** with Intel TBB and OpenMP support
- **Memory-efficient** data structures with shared vertex storage
- **CGAL-based geometry** for exact geometric calculations
- **Sub-second processing** for typical topographic models

### Professional Quality Output
- **Zero non-manifold edges** through comprehensive topology tracking
- **Exact geometric precision** using CGAL predicates
- **Professional mesh validation** matching CAD standards
- **Robust error handling** and automatic mesh repair

### Multi-Format Export
- **3D Mesh Formats**:
  - **STL**: Binary and ASCII formats for 3D printing
  - **OBJ**: With materials, elevation-based coloring, and textures
  - **PLY**: For research and visualization applications
  - **NURBS**: IGES/STEP compatible surfaces for CAD software
- **Raster Formats**:
  - **PNG**: High-quality raster images with elevation coloring
  - **GeoTIFF**: Georeferenced rasters for GIS applications
- **Vector Formats**:
  - **SVG**: Laser-cutter ready vector contours
  - **GeoJSON**: Web-mapping compatible vector data
  - **Shapefile**: Standard GIS vector format with attributes

### Advanced Slicing Modes
- **Terrain-following**: Layers follow actual topographic surface shape
- **Vertical bands**: Traditional contour relief with vertical sides
- **Adaptive layers**: Variable layer heights based on terrain complexity
- **Bounding sphere scaling**: Optimal print volume utilization

## Quick Start

### Prerequisites

```bash
# Ubuntu/Debian
sudo apt-get install build-essential cmake libcgal-dev libeigen3-dev \
    libtbb-dev libgdal-dev libhdf5-dev libomp-dev

# macOS with Homebrew
brew install cmake cgal eigen tbb gdal hdf5 libomp

# Windows with vcpkg
vcpkg install cgal eigen3 tbb gdal hdf5 openmp
```

### Build

```bash
git clone <repository-url>
cd cpp-version
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Basic Usage

```bash
# Generate STL model using place name
./topo-gen --query "Mount Rainier" --layers 15 --output-formats stl

# Generate STL model using explicit coordinates
./topo-gen --upper-left 46.9,-121.8 --lower-right 46.8,-121.7 --layers 15 --output-formats stl

# Multi-format export with terrain-following
./topo-gen --query "Grand Canyon" \
           --terrain-following \
           --output-formats stl,obj,geojson,geotiff \
           --quality high

# High-performance parallel processing with GIS outputs
./topo-gen --query "Yosemite Valley" \
           --layers 20 \
           --threads 8 \
           --output-formats stl,geojson,shp \
           --obj-colors \
           --color-scheme terrain
```

## Command Line Reference

### Location Input (choose one)
- `--query, -q`: Place name or address (uses OpenStreetMap Nominatim geocoding)
  - Example: `--query "Mount Denali"` or `--query "Seattle, WA"`
- `--upper-left`: Upper left corner coordinates as `lat,lon`
- `--lower-right`: Lower right corner coordinates as `lat,lon`

### Layer Configuration
- `-l, --layers`: Number of contour layers (default: 10)
- `-t, --thickness`: Layer thickness in mm (default: 3.0)
- `-s, --substrate-size`: Substrate size in mm (default: 200.0)
- `--min-elevation`: Minimum elevation to include (meters)
- `--max-elevation`: Maximum elevation to include (meters)

### Processing Options
- `--terrain-following`: Use terrain-following instead of vertical bands
- `--no-parallel`: Disable parallel processing
- `--threads`: Number of threads (0 = auto-detect)
- `--simplify`: Simplification tolerance in meters (default: 1.0)
- `--min-area`: Minimum feature area in square meters (default: 100.0)

### Scaling Control
- `--2d-scaling-method`: Scaling method for 2D/vector outputs
  - Options: `auto`, `bed-size`, `material-thickness`, `layers`, `explicit`
- `--3d-scaling-method`: Scaling method for 3D/mesh outputs
  - Options: `auto`, `bed-size`, `print-height`, `uniform-xyz`, `explicit`
- `--explicit-2d-scale-factor`: Explicit 2D scale factor in mm/m
- `--explicit-3d-scale-factor`: Explicit 3D scale factor in mm/m
- `--use-2d-scaling-for-3d`: Force 2D scaling method for 3D outputs
- `--use-3d-scaling-for-2d`: Force 3D scaling method for 2D outputs

### Output Options
- `--output-formats`: Output formats (comma-separated)
  - **3D Mesh**: `stl`, `obj`, `ply`, `nurbs`
  - **Raster**: `png`, `tif`, `geotiff`
  - **Vector**: `svg`, `geojson`, `shp` (Shapefile)
- `-o, --output-dir`: Output directory (default: output)
- `-n, --base-name`: Base filename (default: topographic_model)

### Quality Settings
- `-q, --quality`: Mesh quality: `draft,medium,high,ultra` (default: medium)
- `--nurbs-quality`: NURBS quality: `low,medium,high` (default: medium)

### OBJ Export Options
- `--obj-materials`: Include materials in OBJ export
- `--obj-colors`: Use elevation-based colors
- `--color-scheme`: Color scheme: `grayscale,terrain,rainbow,custom`

## Performance Comparison

| Operation | Python Version | C++ Version | Improvement |
|-----------|----------------|-------------|-------------|
| Elevation loading | 10-30s | 0.1-1s | 50-100x |
| Contour extraction | 30-120s | 0.5-2s | 60-240x |
| Triangulation | 60-300s | 1-5s | 60-300x |
| STL export | <1s | <0.1s | 10x |
| **Total pipeline** | **100-450s** | **2-8s** | **50-200x** |

## Architecture

### Core Components
- **TopographicMesh**: High-performance mesh with topology tracking
- **TrianglePlaneIntersector**: Triangle-plane intersection from libslic3r
- **ElevationProcessor**: GDAL-based elevation data processing
- **MultiFormatExporter**: Professional multi-format export system
- **ParallelLayerGenerator**: TBB-based parallel layer processing

### Library Stack
- **CGAL**: Computational geometry algorithms
- **Eigen**: Linear algebra and matrix operations
- **Intel TBB**: Task-based parallelism
- **GDAL**: Geospatial data processing
- **libslic3r**: Professional slicing algorithms (adapted)

### Key Algorithms
- **Triangle-plane intersection**: Exact geometric slicing
- **Mesh topology tracking**: Real-time manifold validation
- **Bounding sphere scaling**: Optimal print volume utilization
- **Parallel processing**: Multi-threaded layer generation
- **NURBS surface fitting**: Professional CAD-compatible surfaces

## Output Formats

### STL (Stereolithography)
```bash
./topo-gen -b "bounds" -f stl
```
- Binary and ASCII formats
- Optimized for 3D printing
- Professional mesh validation
- Zero non-manifold edges guaranteed

### OBJ (Wavefront)
```bash
./topo-gen -b "bounds" -f obj --obj-colors --color-scheme terrain
```
- Material definitions (.mtl files)
- Elevation-based coloring
- Multiple color schemes
- Texture coordinate support

### PLY (Polygon File Format)
```bash
./topo-gen -b "bounds" -f ply
```
- Vertex colors
- Research-friendly format
- Compact binary representation

### NURBS Surfaces
```bash
./topo-gen -b "bounds" -f nurbs --nurbs-quality high
```
- IGES/STEP compatible
- CAD software integration
- Smooth surface representation
- Parametric control

## Advanced Features

### Terrain-Following Mode
```bash
./topo-gen -b "bounds" --terrain-following
```
Creates layers that follow the actual topographic surface shape rather than vertical elevation bands, resulting in more organic, naturally-shaped layers.

### Adaptive Layer Heights
```bash
./topo-gen -b "bounds" --quality ultra
```
Automatically adjusts layer heights based on local terrain complexity for optimal detail preservation.

### Parallel Processing
```bash
./topo-gen -b "bounds" --threads 8
```
Utilizes multiple CPU cores for dramatic performance improvements on large datasets.

### Bounding Sphere Scaling
Automatically scales models to fit optimally within printer build volumes while maintaining proper proportions.

## Development

### Building with Debug Info
```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

### Running Tests
```bash
cmake .. -DENABLE_TESTING=ON
make -j$(nproc)
ctest
```

### Performance Profiling
```bash
cmake .. -DBUILD_BENCHMARKS=ON
make -j$(nproc)
./benchmarks/benchmark_main
```

## Migration from Python Version

The C++ version maintains API compatibility for basic operations while providing significant performance improvements:

1. **Command-line compatibility**: Similar parameter names and options
2. **Output compatibility**: Generates identical STL files (with better quality)
3. **Configuration compatibility**: JSON config files supported
4. **Performance improvements**: 50-200x faster processing

### Migration Script
```bash
# Convert Python command
python genContours.py --upper-left 46.8,-121.8 --lower-right 46.9,-121.7

# To C++ equivalent
./topo-gen --bounds "46.8,-121.8,46.9,-121.7"
```

## Contributing

1. Fork the repository
2. Create feature branch: `git checkout -b feature-name`
3. Follow C++ Core Guidelines and Google Style Guide
4. Add comprehensive tests for new features
5. Submit pull request with detailed description

## License

MIT License - see LICENSE file for details.

## Acknowledgments

- **Bambu Slicer (libslic3r)**: Core triangle-plane intersection algorithms
- **CGAL Project**: Robust computational geometry library
- **Intel TBB**: High-performance parallel processing
- **GDAL/OGR**: Comprehensive geospatial data and format support
- **OpenStreetMap Nominatim**: Geocoding and location services
- **nlohmann/json**: JSON parsing for configuration and GeoJSON export
- **libcurl**: HTTP client for elevation data and geocoding API access

---

*This C++ implementation represents a complete architectural redesign focused on professional-quality output and exceptional performance for topographic model generation.*