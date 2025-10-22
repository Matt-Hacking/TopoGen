#pragma once

/**
 * @file ExecutionPolicies.hpp
 * @brief Shared execution policy types for cross-platform compatibility
 */

// Check if execution policies are available (GCC 9+, MSVC 2017+, limited in Clang/libc++)
#if defined(__cpp_lib_execution) && __cpp_lib_execution >= 201603L && !defined(__APPLE__)
    #include <execution>
    #define HAS_EXECUTION_POLICY
#endif

namespace topo {

// Simple policy types for cross-platform compatibility
struct SequentialPolicy {};
struct ParallelPolicy {};

} // namespace topo