#!/bin/bash
# Auto-generated environment setup script
# Source this file before building: source build_environment.sh

# macOS Homebrew environment
export PATH="/opt/homebrew/bin:/opt/homebrew/sbin:$PATH"
export CMAKE_PREFIX_PATH="/opt/homebrew:$CMAKE_PREFIX_PATH"
export PKG_CONFIG_PATH="/opt/homebrew/lib/pkgconfig:$PKG_CONFIG_PATH"
export HOMEBREW_PREFIX="/opt/homebrew"

# LLVM/Clang (if using LLVM's clang)
if [ -d "/opt/homebrew/opt/llvm" ]; then
    export LLVM="$(brew --prefix llvm)"
    export PATH="$LLVM/bin:$PATH"
    export LDFLAGS="-L$LLVM/lib $LDFLAGS"
    export CPPFLAGS="-I$LLVM/include $CPPFLAGS"
fi

# OpenMP
if [ -d "/opt/homebrew/opt/libomp" ]; then
    export OpenMP_ROOT="/opt/homebrew/opt/libomp"
fi

echo "Build environment configured!"
echo "Ready to build with: ./scripts/quick-build.sh"
