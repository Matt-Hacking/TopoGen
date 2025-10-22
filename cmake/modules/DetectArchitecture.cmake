# DetectArchitecture.cmake - Detect and normalize architecture information
#
# Sets the following variables:
#   TARGET_ARCHITECTURE - Normalized architecture (x86_64, arm64, etc.)
#   TARGET_PLATFORM - Normalized platform (macos, linux, windows)
#   PACKAGE_ARCHITECTURE - Architecture string for packages (amd64, arm64, x86_64, etc.)

# Detect platform
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(TARGET_PLATFORM "macos")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(TARGET_PLATFORM "linux")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set(TARGET_PLATFORM "windows")
else()
    set(TARGET_PLATFORM "unknown")
endif()

# Detect architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(TARGET_ARCHITECTURE "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(TARGET_ARCHITECTURE "arm64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i386|i686)$")
    set(TARGET_ARCHITECTURE "i686")
else()
    set(TARGET_ARCHITECTURE "${CMAKE_SYSTEM_PROCESSOR}")
endif()

# Set package-specific architecture names
if(TARGET_PLATFORM STREQUAL "linux")
    if(TARGET_ARCHITECTURE STREQUAL "x86_64")
        set(PACKAGE_ARCHITECTURE "amd64")  # DEB uses amd64
        set(RPM_ARCHITECTURE "x86_64")     # RPM uses x86_64
    elseif(TARGET_ARCHITECTURE STREQUAL "arm64")
        set(PACKAGE_ARCHITECTURE "arm64")
        set(RPM_ARCHITECTURE "aarch64")
    else()
        set(PACKAGE_ARCHITECTURE "${TARGET_ARCHITECTURE}")
        set(RPM_ARCHITECTURE "${TARGET_ARCHITECTURE}")
    endif()
elseif(TARGET_PLATFORM STREQUAL "macos")
    set(PACKAGE_ARCHITECTURE "${TARGET_ARCHITECTURE}")
elseif(TARGET_PLATFORM STREQUAL "windows")
    if(TARGET_ARCHITECTURE STREQUAL "x86_64")
        set(PACKAGE_ARCHITECTURE "x64")
    else()
        set(PACKAGE_ARCHITECTURE "${TARGET_ARCHITECTURE}")
    endif()
endif()

# Report detected configuration
message(STATUS "===========================================")
message(STATUS "  Architecture Detection")
message(STATUS "===========================================")
message(STATUS "Platform: ${TARGET_PLATFORM}")
message(STATUS "Architecture: ${TARGET_ARCHITECTURE}")
message(STATUS "Package Architecture: ${PACKAGE_ARCHITECTURE}")
if(DEFINED RPM_ARCHITECTURE)
    message(STATUS "RPM Architecture: ${RPM_ARCHITECTURE}")
endif()

# Check if we're doing a universal build (macOS)
if(CMAKE_OSX_ARCHITECTURES)
    message(STATUS "macOS Architectures: ${CMAKE_OSX_ARCHITECTURES}")
    list(LENGTH CMAKE_OSX_ARCHITECTURES NUM_ARCHS)
    if(NUM_ARCHS GREATER 1)
        message(STATUS "Building Universal Binary")
        set(IS_UNIVERSAL_BINARY TRUE)
    endif()
endif()

message(STATUS "===========================================")

# Export variables
set(TARGET_ARCHITECTURE "${TARGET_ARCHITECTURE}" PARENT_SCOPE)
set(TARGET_PLATFORM "${TARGET_PLATFORM}" PARENT_SCOPE)
set(PACKAGE_ARCHITECTURE "${PACKAGE_ARCHITECTURE}" PARENT_SCOPE)
if(DEFINED RPM_ARCHITECTURE)
    set(RPM_ARCHITECTURE "${RPM_ARCHITECTURE}" PARENT_SCOPE)
endif()
if(DEFINED IS_UNIVERSAL_BINARY)
    set(IS_UNIVERSAL_BINARY "${IS_UNIVERSAL_BINARY}" PARENT_SCOPE)
endif()
