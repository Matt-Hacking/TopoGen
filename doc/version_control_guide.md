# C++ Version Control Guide

This directory includes a sophisticated version control system designed specifically for the C++ topographic generator project.

## Quick Start

### Create a New Version
```bash
./create_version.sh "description_of_changes"
```

### Examples
```bash
# Initial implementation
./create_version.sh "initial_implementation"

# Performance improvements
./create_version.sh "performance_optimizations"

# Bug fixes
./create_version.sh "bug_fixes_and_cleanup"

# New features
./create_version.sh "new_export_formats"

# Build system changes
./create_version.sh "build_system_improvements"
```

## What the Script Does

### 1. **Automatic Version Numbering**
- Scans existing versions in both local and parent version control directories
- Automatically increments to the next available version number
- Handles gaps in version numbering gracefully

### 2. **Intelligent File Filtering**
The script includes **only essential files** for development and deployment:

#### ✅ **Included Files**
- **Source Code**: `src/` directory with all `.cpp` and `.hpp` files
- **Headers**: `include/` directory with header files
- **Build System**: `CMakeLists.txt` files throughout the project
- **Documentation**: All `.md` files (README, BUILD_INSTRUCTIONS, etc.)
- **Scripts**: Shell scripts (`.sh` files) for building and deployment
- **Configuration**: JSON configuration files and project metadata
- **License and Copyright**: Legal and attribution files

#### ❌ **Excluded Files**
- **Build Artifacts**: `build/` directory, compiled objects, executables
- **Output Files**: Generated models (`.stl`, `.svg`, `.obj`, `.ply`, etc.)
- **Test Data**: `test/`, `tests/` directories and log files
- **Cache Data**: `cache/` directory and temporary files
- **IDE Files**: `.vscode/`, `.idea/`, editor swap files
- **Version Archives**: Previous `.zip` files to avoid recursive inclusion
- **System Files**: `.DS_Store`, temporary files, backup files

### 3. **Comprehensive Documentation**
Each version automatically generates detailed notes including:
- **Project Statistics**: File counts, line counts, build components
- **Architecture Overview**: Key components and their purposes
- **Build Instructions**: Step-by-step compilation guide
- **Usage Examples**: Command-line usage demonstrations
- **Dependency Information**: Required and optional libraries
- **Performance Features**: Optimization highlights
- **Compatibility Notes**: Integration with existing workflows

### 4. **Professional Archive Format**
- **Standardized Naming**: `cpp-topographic-generator_v{N}_{description}_{timestamp}.zip`
- **Optimal Size**: Typically 100-200KB for core codebase
- **Self-Contained**: Includes everything needed to build and run
- **Timestamped**: Precise creation time for historical tracking

## Version Control Directory Structure

```
cpp-version/
├── version_control/
│   ├── cpp-topographic-generator_v1_initial_implementation_20250926_132503.zip
│   ├── cpp-topographic-generator_v1_notes.md
│   ├── cpp-topographic-generator_v2_performance_optimizations_20250927_090000.zip
│   ├── cpp-topographic-generator_v2_notes.md
│   └── ...
├── create_version.sh*
├── version_control_guide.md
└── ...
```

## Integration with Main Project

The script is aware of the parent project's version control system and:
- **Respects Main Versions**: Checks `../version_control/version_*.md` files
- **Coordinates Numbering**: Ensures C++ versions don't conflict with main project versions
- **Maintains Compatibility**: Archives can be used alongside main project versions

## Usage Scenarios

### 1. **Development Milestones**
```bash
./create_version.sh "feature_complete_contour_generation"
./create_version.sh "svg_export_implementation"
./create_version.sh "multi_format_export_system"
```

### 2. **Bug Fix Cycles**
```bash
./create_version.sh "cgal_linking_fixes"
./create_version.sh "memory_leak_patches"
./create_version.sh "build_system_stability"
```

### 3. **Performance Iterations**
```bash
./create_version.sh "openmp_parallelization"
./create_version.sh "simd_optimizations"
./create_version.sh "memory_efficiency_improvements"
```

### 4. **Release Preparation**
```bash
./create_version.sh "release_candidate_1"
./create_version.sh "production_ready_build"
./create_version.sh "final_release_v2_0"
```

## Best Practices

### 1. **Descriptive Naming**
- Use clear, specific descriptions
- Separate words with underscores
- Keep descriptions under 50 characters
- Focus on the primary change or achievement

### 2. **Regular Versioning**
- Create versions at logical development milestones
- Version before major refactoring or risky changes
- Version after successful testing or validation
- Version before sharing with others

### 3. **Documentation**
- The generated notes are comprehensive but review them
- Add specific technical details to notes if needed
- Keep track of known issues and limitations
- Document dependencies and build requirements

### 4. **Archive Management**
- Older versions can be moved to long-term storage
- Keep recent 5-10 versions readily accessible
- Archive critical milestone versions permanently
- Use descriptive names to identify important versions quickly

## Recovery and Deployment

### Extract and Build
```bash
# Extract version
unzip cpp-topographic-generator_v1_initial_implementation_20250926_132503.zip
cd cpp-topographic-generator_v1_initial_implementation_20250926_132503

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Test
./topo-gen --help
```

### Deploy to New Environment
```bash
# Copy archive to target system
scp cpp-topographic-generator_v1_*.zip user@target:/tmp/

# SSH to target and deploy
ssh user@target
cd /tmp
unzip cpp-topographic-generator_v1_*.zip
# Follow build instructions
```

## Help and Troubleshooting

### Get Help
```bash
./create_version.sh --help
```

### Common Issues

**"Not in right directory"**: Run script from C++ project root where `CMakeLists.txt` exists

**"Description required"**: Provide a description as the first argument

**"Permission denied"**: Make script executable with `chmod +x create_version.sh`

**"Archive creation failed"**: Check disk space and write permissions

### Script Features

- **Automatic Validation**: Checks directory structure and requirements
- **Error Handling**: Clear error messages and graceful failure
- **Progress Reporting**: Detailed logging of each step
- **Size Optimization**: Efficient compression and file selection
- **Cross-Platform**: Works on macOS, Linux, and Windows (with bash)

This version control system provides professional-grade project management for the C++ topographic generator, ensuring reliable development history and easy deployment across environments.