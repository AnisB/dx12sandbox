# GLFW_FOUND
# GLFW_INCLUDE_DIR
# GLFW_LIBRARY

IF (WIN32)
  FIND_PATH(GLFW_INCLUDE_DIR GLFW/glfw3.h DOC "Path to GLFW include directory." HINTS ${BENTO_THIRD_INCLUDE_DIRS})
  FIND_LIBRARY(GLFW_LIBRARY DOC "Absolute path to GLFW library." NAMES glfw3.lib HINTS ${BENTO_THIRD_LIBRARY_DIRS}/glfw/)
ELSE (WIN32)
  FIND_PATH(GLFW_INCLUDE_DIR GLFW/glfw3.h DOC "Path to GLFW include directory."
    HINTS
    $ENV{GLFW_ROOT}
    PATH_SUFFIX include
    PATHS
    /usr/include/
    /usr/local/include/
    # By default headers are under GL subfolder
    /usr/include/GL
    /usr/local/include/GL
    ${GLFW_ROOT_DIR}/include/ # added by ptr
  )
  FIND_LIBRARY(GLFW_LIBRARY DOC "Absolute path to GLFW library."
    NAMES glfw GLFW.lib libglfw3.a glfw3.lib
    HINTS
    $ENV{GLFW_ROOT}
    # In the expanded GLFW source archive. Should be uncommon, but whatever.
    PATH_SUFFIXES lib/win32 lib/cocoa lib/x11
    PATHS
    /usr/local/lib
    /usr/lib
    ${GLFW_ROOT_DIR}/lib-msvc100/release # added by ptr
  )
ENDIF (WIN32)

SET(GLFW_FOUND "NO")
IF(GLFW_LIBRARY AND GLFW_INCLUDE_DIR)
  SET(GLFW_FOUND "YES")
  message(STATUS "Found GLFW: ${GLFW_LIBRARY}")
ENDIF(GLFW_LIBRARY AND GLFW_INCLUDE_DIR)