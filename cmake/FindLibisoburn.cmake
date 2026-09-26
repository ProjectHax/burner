# FindLibisoburn.cmake
# Find libisoburn from the Libburnia project
#
# This module defines:
#   Libisoburn_FOUND        - True if libisoburn was found
#   Libisoburn_INCLUDE_DIRS - Include directories
#   Libisoburn_LIBRARIES    - Libraries to link
#   Libisoburn::libisoburn  - Imported target

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_LIBISOBURN QUIET libisoburn-1)
endif()

find_path(LIBISOBURN_INCLUDE_DIR
    NAMES libisoburn/libisoburn.h
    HINTS ${PC_LIBISOBURN_INCLUDE_DIRS}
    PATH_SUFFIXES include
)

find_library(LIBISOBURN_LIBRARY
    NAMES isoburn
    HINTS ${PC_LIBISOBURN_LIBRARY_DIRS}
)

# Get version from pkg-config
if(PC_LIBISOBURN_VERSION)
    set(Libisoburn_VERSION ${PC_LIBISOBURN_VERSION})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libisoburn
    REQUIRED_VARS LIBISOBURN_LIBRARY LIBISOBURN_INCLUDE_DIR
    VERSION_VAR Libisoburn_VERSION
)

if(Libisoburn_FOUND)
    set(Libisoburn_LIBRARIES ${LIBISOBURN_LIBRARY})
    set(Libisoburn_INCLUDE_DIRS ${LIBISOBURN_INCLUDE_DIR})

    if(NOT TARGET Libisoburn::libisoburn)
        add_library(Libisoburn::libisoburn UNKNOWN IMPORTED)
        set_target_properties(Libisoburn::libisoburn PROPERTIES
            IMPORTED_LOCATION "${LIBISOBURN_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBISOBURN_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(LIBISOBURN_INCLUDE_DIR LIBISOBURN_LIBRARY)
