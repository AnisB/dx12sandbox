# GLEW_FOUND
# GLEW_INCLUDE_PATH
# GLEW_LIBRARY

IF (WIN32)
	FIND_PATH( GLEW_INCLUDE_PATH GL/glew.h DOC "The directory where GL/glew.h resides" HINTS ${BENTO_THIRD_INCLUDE_DIRS})
	FIND_LIBRARY(GLEW_LIBRARY NAMES glew32.lib PATHS ${BENTO_THIRD_LIBRARY_DIRS} DOC "The GLEW library")
ELSE (WIN32)
	FIND_PATH( GLEW_INCLUDE_PATH GL/glew.h
		/usr/include
		/usr/local/include
		/sw/include
		/opt/local/include
		DOC "The directory where GL/glew.h resides")
	FIND_LIBRARY( GLEW_LIBRARY
		NAMES GLEW glew
		PATHS
		${PROJECT_SOURCE_DIR}/external/mac/glew
		/usr/lib64
		/usr/lib
		/usr/local/lib64
		/usr/local/lib
		/sw/lib
		/opt/local/lib
		DOC "The GLEW library")
ENDIF (WIN32)

SET(GLEW_FOUND "NO")
IF(GLEW_LIBRARY AND GLEW_INCLUDE_PATH)
  SET(GLEW_FOUND "YES")
  message(STATUS "Found GLEW: ${GLEW_LIBRARY}")
ENDIF(GLEW_LIBRARY AND GLEW_INCLUDE_PATH)

MARK_AS_ADVANCED( GLEW_FOUND )