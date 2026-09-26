# src/CMakeLists.txt

# Find curl for MusicBrainz API (optional)
find_package(PkgConfig REQUIRED)
pkg_check_modules(CURL libcurl)
if(CURL_FOUND)
    message(STATUS "libcurl found - MusicBrainz metadata lookup enabled")
    add_definitions(-DHAVE_LIBCURL)
else()
    message(STATUS "libcurl not found - MusicBrainz metadata lookup disabled")
endif()

# Core library (non-Qt, pure C++20)
add_library(burner_core STATIC
    core/libburnia/BurniaInit.cpp
    core/libburnia/DriveHandle.cpp
    core/libburnia/IsoImageHandle.cpp
    core/DriveManager.cpp
    core/ImageBuilder.cpp
    core/BurnEngine.cpp
    core/AudioEncoder.cpp
    core/DiscCloner.cpp
    core/VcdBuilder.cpp
    core/CdReader.cpp
    core/AudioOutputEncoder.cpp
    core/MetadataProvider.cpp
    core/FileNamingPattern.cpp
    core/CdRipper.cpp
    core/CueSheet.cpp
    core/ChecksumCalculator.cpp
    core/IsoReader.cpp
)

target_include_directories(burner_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(burner_core PUBLIC
    Libburn::libburn
    Libisofs::libisofs
    Libisoburn::libisoburn
    PkgConfig::AVCODEC
    PkgConfig::AVFORMAT
    PkgConfig::AVUTIL
    PkgConfig::SWRESAMPLE
    PkgConfig::SWSCALE
    OpenSSL::Crypto
)

if(CURL_FOUND)
    target_link_libraries(burner_core PUBLIC ${CURL_LIBRARIES})
    target_include_directories(burner_core PUBLIC ${CURL_INCLUDE_DIRS})
endif()

# Engine library (Qt integration)
add_library(burner_engine STATIC
    engine/QtBurnEngine.cpp
    engine/DriveMonitor.cpp
    engine/BurnWorker.cpp
    engine/RipWorker.cpp
    engine/QtRipEngine.cpp
    engine/ChecksumWorker.cpp
)

target_link_libraries(burner_engine PUBLIC
    burner_core
    Qt6::Core
)

# Models library
add_library(burner_models STATIC
    models/DriveListModel.cpp
    models/FileTreeModel.cpp
    models/TrackListModel.cpp
    models/CdTrackModel.cpp
    models/IsoTreeModel.cpp
)

target_link_libraries(burner_models PUBLIC
    burner_core
    Qt6::Core
    Qt6::Gui
)

# UI library
add_library(burner_ui STATIC
    ui/ThemeManager.cpp
    ui/MainWindow.cpp
    ui/Dialogs.cpp
    ui/widgets/TitleBar.cpp
    ui/widgets/WindowChrome.cpp
    ui/widgets/DriveSelector.cpp
    ui/widgets/CapacityBar.cpp
    ui/widgets/FileDropArea.cpp
    ui/widgets/BurnLog.cpp
    ui/pages/DataDiscPage.cpp
    ui/pages/AudioCdPage.cpp
    ui/pages/VideoCdPage.cpp
    ui/pages/IsoWriterPage.cpp
    ui/pages/DiscClonerPage.cpp
    ui/pages/CdRipperPage.cpp
    ui/dialogs/BurnOptionsDialog.cpp
    ui/dialogs/BurnProgressDialog.cpp
    ui/dialogs/RipProgressDialog.cpp
    ui/dialogs/ChecksumDialog.cpp
    ui/pages/IsoBrowserPage.cpp
)

target_link_libraries(burner_ui PUBLIC
    burner_engine
    burner_models
    Qt6::Widgets
    Qt6::Gui
)

# Optional: ask the Wayland compositor whether it draws title bars (see WindowChrome)
pkg_check_modules(WAYLAND_CLIENT IMPORTED_TARGET wayland-client)
if(WAYLAND_CLIENT_FOUND)
    target_link_libraries(burner_ui PRIVATE PkgConfig::WAYLAND_CLIENT)
    target_compile_definitions(burner_ui PRIVATE BURNER_HAVE_WAYLAND_CLIENT)
endif()

# Optional: Qt6 Multimedia for sound notifications
if(Qt6Multimedia_FOUND)
    target_link_libraries(burner_ui PUBLIC Qt6::Multimedia)
    target_compile_definitions(burner_ui PUBLIC HAVE_QT_MULTIMEDIA)
endif()

# Main executable
qt_add_executable(burner
    main.cpp
    ${CMAKE_SOURCE_DIR}/resources/resources.qrc
)

target_link_libraries(burner PRIVATE
    burner_ui
)

# Translations - compile .ts to .qm files (requires Qt6LinguistTools)
if(Qt6LinguistTools_FOUND)
    set(TS_FILES
        ${CMAKE_SOURCE_DIR}/translations/burner_en.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_es.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_fr.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_de.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_it.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_pt.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_ru.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_zh_CN.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_zh_TW.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_ja.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_ko.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_ar.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_nl.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_pl.ts
        ${CMAKE_SOURCE_DIR}/translations/burner_tr.ts
    )

    # Compile translations - output to source translations directory
    # so the app can load them from there during development
    set(TRANSLATION_OUTPUT_DIR ${CMAKE_SOURCE_DIR}/translations)

    foreach(TS_FILE ${TS_FILES})
        get_filename_component(TS_NAME ${TS_FILE} NAME_WE)
        set(QM_FILE ${TRANSLATION_OUTPUT_DIR}/${TS_NAME}.qm)
        add_custom_command(
            OUTPUT ${QM_FILE}
            COMMAND Qt6::lrelease ${TS_FILE} -qm ${QM_FILE}
            DEPENDS ${TS_FILE}
            COMMENT "Compiling ${TS_NAME}.ts"
            VERBATIM
        )
        list(APPEND QM_FILES ${QM_FILE})
    endforeach()

    add_custom_target(translations ALL DEPENDS ${QM_FILES})
    add_dependencies(burner translations)
endif()

# Import static platform plugins (Qt6 static build)
qt_import_plugins(burner
    INCLUDE Qt6::QXcbIntegrationPlugin
)

# Try to include Wayland plugin if available
if(TARGET Qt6::QWaylandIntegrationPlugin)
    qt_import_plugins(burner
        INCLUDE Qt6::QWaylandIntegrationPlugin
    )
endif()

set_target_properties(burner PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
