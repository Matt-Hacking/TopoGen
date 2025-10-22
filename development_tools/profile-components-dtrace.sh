#!/bin/bash
# ============================================================================
# Component-Level Profiling using DTrace (macOS)
# ============================================================================
# Deep-dive profiling into individual components using DTrace
# Requires sudo permissions on macOS
# ============================================================================

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
PROFILE_DIR="$PROJECT_DIR/fixes/audit-results"
DTRACE_OUTPUT="$PROFILE_DIR/component-dtrace.txt"
EXECUTABLE="$PROJECT_DIR/build/topo-gen"

# Test parameters
TEST_UPPER_LEFT="47.6062,-122.3321"
TEST_LOWER_RIGHT="47.6020,-122.3280"
TEST_NUM_LAYERS="5"  # Fewer layers for quicker profiling
TEST_SUBSTRATE="100,100"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

log_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

# ============================================================================
# DTrace Scripts
# ============================================================================

create_dtrace_script() {
    cat > /tmp/topo_profile.d << 'EOF'
#!/usr/sbin/dtrace -s

/* Track function entry/exit for key components */

pid$target::*ElevationProcessor*:entry,
pid$target::*ContourGenerator*:entry,
pid$target::*TrianglePlane*:entry,
pid$target::*TopographicMesh*:entry,
pid$target::*SVGExporter*:entry,
pid$target::*STLExporter*:entry,
pid$target::*SRTMDownloader*:entry,
pid$target::*OSMTileCache*:entry
{
    self->start[probefunc] = timestamp;
    @entries[probefunc] = count();
}

pid$target::*ElevationProcessor*:return,
pid$target::*ContourGenerator*:return,
pid$target::*TrianglePlane*:return,
pid$target::*TopographicMesh*:return,
pid$target::*SVGExporter*:return,
pid$target::*STLExporter*:return,
pid$target::*SRTMDownloader*:return,
pid$target::*OSMTileCache*:return
/self->start[probefunc]/
{
    @time[probefunc] = sum(timestamp - self->start[probefunc]);
    @avg_time[probefunc] = avg(timestamp - self->start[probefunc]);
    self->start[probefunc] = 0;
}

/* Track memory allocations */
pid$target::malloc:entry
{
    self->size = arg0;
}

pid$target::malloc:return
/self->size/
{
    @malloc_sizes = quantize(self->size);
    @malloc_count = count();
    @malloc_total = sum(self->size);
    self->size = 0;
}

/* Track file I/O */
syscall::open*:entry
/pid == $target/
{
    @files[copyinstr(arg0)] = count();
}

syscall::read:entry,
syscall::write:entry
/pid == $target/
{
    @io_ops[probefunc] = count();
    @io_bytes[probefunc] = sum(arg2);
}

/* Print results on exit */
END
{
    printf("\n=== Function Call Counts ===\n");
    printa("%s: %@d calls\n", @entries);

    printf("\n=== Total Time per Function (nanoseconds) ===\n");
    printa("%s: %@d ns\n", @time);

    printf("\n=== Average Time per Function (nanoseconds) ===\n");
    printa("%s: %@d ns\n", @avg_time);

    printf("\n=== Memory Allocation Distribution ===\n");
    printa(@malloc_sizes);

    printf("\n=== Memory Statistics ===\n");
    printa("Total allocations: %@d\n", @malloc_count);
    printa("Total bytes allocated: %@d\n", @malloc_total);

    printf("\n=== File Access ===\n");
    printa("%s: %@d opens\n", @files);

    printf("\n=== I/O Operations ===\n");
    printa("%s: %@d ops, %@d bytes\n", @io_ops, @io_bytes);
}
EOF
}

# ============================================================================
# Sampling Profiler Script
# ============================================================================

create_sample_script() {
    cat > /tmp/topo_sample.d << 'EOF'
#!/usr/sbin/dtrace -s

profile-997
/pid == $target/
{
    @samples[ustack()] = count();
}

tick-10s
{
    printf("=== Stack samples at %Y ===\n", walltimestamp);
    trunc(@samples, 10);
    printa(@samples);
    clear(@samples);
}

END
{
    printf("\n=== Final Stack Sample Summary ===\n");
    trunc(@samples, 20);
    printa(@samples);
}
EOF
}

# ============================================================================
# Main Profiling Function
# ============================================================================

profile_with_dtrace() {
    log_info "Component profiling with DTrace..."

    # Check if we can use dtrace
    if ! command -v dtrace &> /dev/null; then
        log_warning "DTrace not available on this system"
        return 1
    fi

    # Check if SIP allows dtrace
    if csrutil status | grep -q "enabled"; then
        log_warning "System Integrity Protection may prevent DTrace usage"
        log_warning "You may need to disable SIP or use other profiling methods"
    fi

    # Create dtrace scripts
    create_dtrace_script
    create_sample_script

    # Build command
    local cmd="$EXECUTABLE"
    cmd="$cmd --upper-left $TEST_UPPER_LEFT"
    cmd="$cmd --lower-right $TEST_LOWER_RIGHT"
    cmd="$cmd --num-layers $TEST_NUM_LAYERS"
    cmd="$cmd --substrate-size $TEST_SUBSTRATE"
    cmd="$cmd --output-base /tmp/dtrace_test"
    cmd="$cmd --output-format svg"

    log_info "Running with DTrace monitoring..."
    log_info "Command: $cmd"

    # Run with dtrace (requires sudo)
    if [ "$EUID" -ne 0 ]; then
        log_warning "DTrace requires sudo permissions. You'll be prompted for your password."
    fi

    # Execute with dtrace
    sudo dtrace -s /tmp/topo_profile.d -c "$cmd" > "$DTRACE_OUTPUT" 2>&1

    log_info "DTrace output saved to: $DTRACE_OUTPUT"

    # Clean up
    rm -f /tmp/topo_profile.d /tmp/topo_sample.d
    rm -rf /tmp/dtrace_test*
}

# ============================================================================
# Alternative: Sample-based profiling without sudo
# ============================================================================

profile_with_sample() {
    log_info "Using sample-based profiling (no sudo required)..."

    # Use the sample command on macOS
    if command -v sample &> /dev/null; then
        local cmd="$EXECUTABLE"
        cmd="$cmd --upper-left $TEST_UPPER_LEFT"
        cmd="$cmd --lower-right $TEST_LOWER_RIGHT"
        cmd="$cmd --num-layers $TEST_NUM_LAYERS"
        cmd="$cmd --substrate-size $TEST_SUBSTRATE"
        cmd="$cmd --output-base /tmp/sample_test"
        cmd="$cmd --output-format svg"

        log_info "Running with sample profiler..."

        # Start the process in background
        $cmd &
        local pid=$!

        # Sample it
        sample $pid 10 -file "$PROFILE_DIR/component-sample.txt" &> /dev/null

        # Wait for completion
        wait $pid

        log_info "Sample output saved to: $PROFILE_DIR/component-sample.txt"

        # Clean up
        rm -rf /tmp/sample_test*
    else
        log_warning "sample command not available"
    fi
}

# ============================================================================
# Instruments-based profiling (if available)
# ============================================================================

profile_with_instruments() {
    log_info "Checking for Instruments availability..."

    if command -v xcrun &> /dev/null && xcrun instruments -s &> /dev/null; then
        log_info "Instruments is available. Creating template..."

        # Create a simple Instruments trace
        local trace_file="$PROFILE_DIR/component-instruments.trace"

        local cmd="$EXECUTABLE"
        cmd="$cmd --upper-left $TEST_UPPER_LEFT"
        cmd="$cmd --lower-right $TEST_LOWER_RIGHT"
        cmd="$cmd --num-layers $TEST_NUM_LAYERS"
        cmd="$cmd --substrate-size $TEST_SUBSTRATE"
        cmd="$cmd --output-base /tmp/instruments_test"
        cmd="$cmd --output-format svg"

        log_info "Running with Instruments Time Profiler..."

        # Use Instruments command line (may require developer tools)
        xcrun xctrace record --template "Time Profiler" --launch -- $cmd --output "$trace_file" 2>/dev/null || {
            log_warning "Instruments profiling failed. This may require Xcode."
        }

        if [ -f "$trace_file" ]; then
            log_info "Instruments trace saved to: $trace_file"
            log_info "Open with: open $trace_file"
        fi

        # Clean up
        rm -rf /tmp/instruments_test*
    else
        log_warning "Xcode Instruments not available"
    fi
}

# ============================================================================
# Main
# ============================================================================

main() {
    echo "=============================================="
    echo "   Component-Level Profiler"
    echo "   C++ Topographic Generator"
    echo "=============================================="
    echo ""

    mkdir -p "$PROFILE_DIR"

    # Check executable exists
    if [ ! -f "$EXECUTABLE" ]; then
        log_warning "Executable not found. Build it first with:"
        echo "  cd $PROJECT_DIR && ./scripts/quick-build.sh"
        exit 1
    fi

    # Try different profiling methods
    echo ""
    echo "Available profiling methods:"
    echo "1. DTrace (requires sudo)"
    echo "2. Sample (no sudo required)"
    echo "3. Instruments (requires Xcode)"
    echo ""

    # Try sample-based first (doesn't require sudo)
    profile_with_sample

    # Try Instruments if available
    profile_with_instruments

    # Ask about DTrace
    read -p "Run DTrace profiling? (requires sudo) [y/N]: " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        profile_with_dtrace
    fi

    echo ""
    echo "=============================================="
    echo "   Component Profiling Complete!"
    echo "=============================================="
    echo ""
    echo "📊 Results saved to:"
    echo "   $PROFILE_DIR/"
    echo ""

    return 0
}

# Execute
main "$@"