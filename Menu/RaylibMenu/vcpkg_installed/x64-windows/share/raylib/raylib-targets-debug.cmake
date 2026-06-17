#----------------------------------------------------------------
# Generated CMake target import file for configuration "Debug".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "raylib" for configuration "Debug"
set_property(TARGET raylib APPEND PROPERTY IMPORTED_CONFIGURATIONS DEBUG)
set_target_properties(raylib PROPERTIES
  IMPORTED_IMPLIB_DEBUG "${_IMPORT_PREFIX}/debug/lib/raylib.lib"
  IMPORTED_LINK_DEPENDENT_LIBRARIES_DEBUG "glfw"
  IMPORTED_LOCATION_DEBUG "${_IMPORT_PREFIX}/debug/bin/raylib.dll"
  )

list(APPEND _cmake_import_check_targets raylib )
list(APPEND _cmake_import_check_files_for_raylib "${_IMPORT_PREFIX}/debug/lib/raylib.lib" "${_IMPORT_PREFIX}/debug/bin/raylib.dll" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
