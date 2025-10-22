# Development Tools

This directory contains scripts used during development for profiling, debugging, and performance optimization. These tools are not required for normal use of the topographic generator but are preserved for future development work.

## Profiling Scripts

### `profile-build.sh`
Measures build performance and identifies bottlenecks in the compilation process.

**Usage:**
```bash
./profile-build.sh
```

**Output:** Creates `build-profile-results.txt` with timing analysis.

### `profile-memory.sh`
Profiles memory usage during execution using system tools.

**Platform:** macOS (uses Instruments/leaks)

### `profile-comprehensive.sh`
Runs a comprehensive profiling suite including CPU, memory, and I/O profiling.

**Platform:** macOS (uses Instruments)

### `profile-components-dtrace.sh`
Uses DTrace to profile individual components and system calls.

**Platform:** macOS (requires DTrace)

### `profile-baseline-simple.sh`
Simple baseline profiling for quick performance checks.

## Development Utilities

### `do_next_run.sh`
Ad-hoc test runner used during development with hardcoded test parameters. Useful as a template for custom test runs.

**Note:** This script has hardcoded paths and is specific to development workflows.

---

## Moving Scripts Back to Production

If any of these scripts should be promoted to production use:

1. Review and update hardcoded paths
2. Add comprehensive error handling
3. Add command-line argument parsing
4. Update documentation
5. Test on multiple platforms (if applicable)
6. Move to `scripts/` directory

---

**Note:** These scripts are preserved for reference and future development. They are not maintained as part of the production release and may contain platform-specific code or hardcoded values.
