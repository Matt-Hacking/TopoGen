# CLI interface library

add_library(TopoCLI STATIC
    CommandLineInterface.cpp
    ConfigurationManager.cpp
    ExportOrchestrator.cpp
    SimpleCommandLineParser.hpp
)

target_include_directories(TopoCLI
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${CMAKE_BINARY_DIR}  # For generated version.h
)

target_link_libraries(TopoCLI
    PRIVATE
        TopoCore
        TopoExport  # Required by ExportOrchestrator
)

target_compile_features(TopoCLI PUBLIC cxx_std_20)

# Add compiler flag for std::string::starts_with (C++20 feature)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS "12.0")
    target_compile_definitions(TopoCLI PRIVATE USE_STRING_STARTS_WITH_FALLBACK)
endif()