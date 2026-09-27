cmake_minimum_required(VERSION 3.21)

# The release version lives in the VERSION file; bump it for each release
file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" BURNER_VERSION LIMIT_COUNT 1)

project(Burner
    VERSION ${BURNER_VERSION}
    DESCRIPTION "Qt6 CD/DVD/BD Burning Application"
    LANGUAGES CXX
)

# Re-run CMake when VERSION changes, and expose it to the code
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS VERSION)
add_compile_definitions(BURNER_VERSION="${PROJECT_VERSION}")

# Default to Release build with optimizations
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Qt6 configuration
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Custom find module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Find Qt6
find_package(Qt6 6.2 REQUIRED COMPONENTS
    Core
    Widgets
    Gui
)

# LinguistTools is optional (needed for compiling translations)
find_package(Qt6 6.2 QUIET COMPONENTS LinguistTools)
if(Qt6LinguistTools_FOUND)
    message(STATUS "Qt6 LinguistTools found - translations will be compiled")
else()
    message(STATUS "Qt6 LinguistTools not found - install qt6-qttools-devel to compile translations")
endif()

# Multimedia is optional (needed for sound notifications)
find_package(Qt6 6.2 QUIET COMPONENTS Multimedia)
if(Qt6Multimedia_FOUND)
    message(STATUS "Qt6 Multimedia found - sound notifications enabled")
else()
    message(STATUS "Qt6 Multimedia not found - sound notifications disabled")
endif()

# Find Libburnia libraries
find_package(Libburn REQUIRED)
find_package(Libisofs REQUIRED)
find_package(Libisoburn REQUIRED)

# Find FFmpeg for audio/video encoding
find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)
pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)

# Find OpenSSL for SHA-1 (MusicBrainz disc ID calculation)
find_package(OpenSSL REQUIRED)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
        -Wno-unused-parameter
    )
endif()

# Add source directory
add_subdirectory(src)

# Tests (optional)
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS burner
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install desktop file and icon
install(FILES packaging/burner.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
install(FILES packaging/burner.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

# Install translations (if .qm files exist)
install(DIRECTORY translations/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/burner/translations
    OPTIONAL
    FILES_MATCHING PATTERN "*.qm"
)

# CPack configuration for source packages
set(CPACK_PACKAGE_NAME "burner")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${PROJECT_DESCRIPTION})
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
    "/build/"
    "/\\\\.git/"
    "/\\\\.claude/"
    "\\\\.AppImage$"
    "rpmbuild/"
)
include(CPack)
