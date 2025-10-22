# GUI Application CMake configuration
# Requires Qt6 Components: Core Widgets Svg Network PrintSupport

# Collect GUI source files
set(GUI_SOURCES
    main.cpp
    MainWindow.cpp

    # Widgets
    widgets/MapWidget.cpp
    widgets/ZoomControls.cpp
    widgets/PreviewDialog.cpp

    # Panels
    panels/MapFeaturesPanel.cpp
    panels/RasterConfigPanel.cpp
    panels/VectorConfigPanel.cpp
    panels/MeshConfigPanel.cpp

    # Dialogs
    dialogs/PreferencesDialog.cpp
    dialogs/GenerationProgressDialog.cpp
    dialogs/FirstRunDialog.cpp

    # Workers
    workers/GenerationWorker.cpp
    workers/TilePreloadWorker.cpp

    # Utilities
    utils/TileDownloader.cpp
    utils/MapLabelManager.cpp
    utils/NominatimClient.cpp
    utils/StateManager.cpp
)

set(GUI_HEADERS
    MainWindow.hpp

    # Widgets
    widgets/MapWidget.hpp
    widgets/ZoomControls.hpp
    widgets/PreviewDialog.hpp

    # Panels
    panels/MapFeaturesPanel.hpp
    panels/RasterConfigPanel.hpp
    panels/VectorConfigPanel.hpp
    panels/MeshConfigPanel.hpp

    # Dialogs
    dialogs/PreferencesDialog.hpp
    dialogs/GenerationProgressDialog.hpp
    dialogs/FirstRunDialog.hpp

    # Workers
    workers/GenerationWorker.hpp
    workers/TilePreloadWorker.hpp

    # Utilities
    utils/TileDownloader.hpp
    utils/MapLabelManager.hpp
    utils/MapBridge.hpp
    utils/NominatimClient.hpp
    utils/StateManager.hpp
)

# Qt resources (HTML, icons, etc.)
set(GUI_RESOURCES
    resources/resources.qrc
)

# Create GUI executable
add_executable(topo-gen-gui
    ${GUI_SOURCES}
    ${GUI_HEADERS}
    ${GUI_RESOURCES}
)

# Link libraries
target_link_libraries(topo-gen-gui
    PRIVATE
    TopoCore
    TopoExport
    Qt6::Core
    Qt6::Widgets
    Qt6::Svg
    Qt6::SvgWidgets
    Qt6::Network
    Qt6::PrintSupport
    Qt6::WebEngineWidgets
    Qt6::WebChannel
)

# Include directories
target_include_directories(topo-gen-gui
    PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/widgets
    ${CMAKE_CURRENT_SOURCE_DIR}/panels
    ${CMAKE_CURRENT_SOURCE_DIR}/dialogs
    ${CMAKE_CURRENT_SOURCE_DIR}/workers
    ${CMAKE_CURRENT_SOURCE_DIR}/utils
)

# macOS bundle configuration
if(APPLE)
    set_target_properties(topo-gen-gui PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.matthewblock.topographic-generator"
        MACOSX_BUNDLE_BUNDLE_NAME "Topographic Generator"
        MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
        MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
        MACOSX_BUNDLE_INFO_STRING "High-performance topographic model generator"
        MACOSX_BUNDLE_COPYRIGHT "Copyright © 2025 Matthew Block. MIT License."
    )

    # macOS: Remove extended attributes and code sign app bundle after build
    # This prevents "Code Signature Invalid" errors when building on network volumes
    # Strategy: Copy to /tmp (local volume), clean and sign there, then copy back
    add_custom_command(TARGET topo-gen-gui POST_BUILD
        COMMAND cp -R $<TARGET_BUNDLE_DIR:topo-gen-gui> /tmp/topo-gen-gui-temp.app
        COMMAND xattr -cr /tmp/topo-gen-gui-temp.app
        COMMAND codesign --force --deep --sign - /tmp/topo-gen-gui-temp.app
        COMMAND rm -rf $<TARGET_BUNDLE_DIR:topo-gen-gui>
        COMMAND cp -R /tmp/topo-gen-gui-temp.app $<TARGET_BUNDLE_DIR:topo-gen-gui>
        COMMAND rm -rf /tmp/topo-gen-gui-temp.app
        COMMENT "Code-signing GUI app bundle (via /tmp to avoid network volume issues)"
        VERBATIM
    )
endif()

# Windows-specific settings
if(WIN32)
    # Don't show console window for GUI app
    set_target_properties(topo-gen-gui PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# Installation
install(TARGETS topo-gen-gui
    BUNDLE DESTINATION . COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
)

message(STATUS "Configured GUI application: topo-gen-gui")
