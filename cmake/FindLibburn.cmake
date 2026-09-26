# FindLibburn.cmake
# Find libburn from the Libburnia project
#
# This module defines:
#   Libburn_FOUND        - True if libburn was found
#   Libburn_INCLUDE_DIRS - Include directories
#   Libburn_LIBRARIES    - Libraries to link
#   Libburn::libburn     - Imported target

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_LIBBURN QUIET libburn-1)
endif()

find_path(LIBBURN_INCLUDE_DIR
    NAMES libburn/libburn.h
    HINTS ${PC_LIBBURN_INCLUDE_DIRS}
    PATH_SUFFIXES include
)

find_library(LIBBURN_LIBRARY
    NAMES burn
    HINTS ${PC_LIBBURN_LIBRARY_DIRS}
)

# Get version from pkg-config or header
if(PC_LIBBURN_VERSION)
    set(Libburn_VERSION ${PC_LIBBURN_VERSION})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libburn
    REQUIRED_VARS LIBBURN_LIBRARY LIBBURN_INCLUDE_DIR
    VERSION_VAR Libburn_VERSION
)

if(Libburn_FOUND)
    set(Libburn_LIBRARIES ${LIBBURN_LIBRARY})
    set(Libburn_INCLUDE_DIRS ${LIBBURN_INCLUDE_DIR})

    if(NOT TARGET Libburn::libburn)
        add_library(Libburn::libburn UNKNOWN IMPORTED)
        set_target_properties(Libburn::libburn PROPERTIES
            IMPORTED_LOCATION "${LIBBURN_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBBURN_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(LIBBURN_INCLUDE_DIR LIBBURN_LIBRARY)
