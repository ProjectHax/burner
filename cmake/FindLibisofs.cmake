# FindLibisofs.cmake
# Find libisofs from the Libburnia project
#
# This module defines:
#   Libisofs_FOUND        - True if libisofs was found
#   Libisofs_INCLUDE_DIRS - Include directories
#   Libisofs_LIBRARIES    - Libraries to link
#   Libisofs::libisofs    - Imported target

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PC_LIBISOFS QUIET libisofs-1)
endif()

find_path(LIBISOFS_INCLUDE_DIR
    NAMES libisofs/libisofs.h
    HINTS ${PC_LIBISOFS_INCLUDE_DIRS}
    PATH_SUFFIXES include
)

find_library(LIBISOFS_LIBRARY
    NAMES isofs
    HINTS ${PC_LIBISOFS_LIBRARY_DIRS}
)

# Get version from pkg-config
if(PC_LIBISOFS_VERSION)
    set(Libisofs_VERSION ${PC_LIBISOFS_VERSION})
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libisofs
    REQUIRED_VARS LIBISOFS_LIBRARY LIBISOFS_INCLUDE_DIR
    VERSION_VAR Libisofs_VERSION
)

if(Libisofs_FOUND)
    set(Libisofs_LIBRARIES ${LIBISOFS_LIBRARY})
    set(Libisofs_INCLUDE_DIRS ${LIBISOFS_INCLUDE_DIR})

    if(NOT TARGET Libisofs::libisofs)
        add_library(Libisofs::libisofs UNKNOWN IMPORTED)
        set_target_properties(Libisofs::libisofs PROPERTIES
            IMPORTED_LOCATION "${LIBISOFS_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBISOFS_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(LIBISOFS_INCLUDE_DIR LIBISOFS_LIBRARY)
