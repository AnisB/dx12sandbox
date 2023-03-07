cmake_minimum_required(VERSION 3.2)
project(graphics_sandbox)

# Declare the global variables for the cmake project
set(BENTO_SDK_ROOT ${PROJECT_SOURCE_DIR}/bento)
set(GRAPHICS_SANDBOX_SDK_ROOT ${PROJECT_SOURCE_DIR}/sdk)
set(GRAPHICS_SANDBOX_TESTS_ROOT ${PROJECT_SOURCE_DIR}/tests)
set(GRAPHICS_SANDBOX_3RD_INCLUDES ${PROJECT_SOURCE_DIR}/3rd/win32/include)

# Set the cmake path variable
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# This flag must be activated in order to handle properly folder flags
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Define the cmake macros
include(CMakePlatforms)
include(CMakeMacros)
include(CMakeBuildSettings)

# Define the build options
define_plaform_settings()

# Print the platform's name
message(STATUS "The build identifier is: ${GRAPHICS_SANDBOX_PLATFORM_NAME}")

# Create the list of allowed files to be included
set(bento_source_extensions)
list(APPEND bento_source_extensions ".h" ".cpp" ".inl" ".txt")

# Compile the bento SDK
add_subdirectory(${BENTO_SDK_ROOT}/src)

# Generate the graphics_sandbox SDK
add_subdirectory(${GRAPHICS_SANDBOX_SDK_ROOT}/src)

# Adding the applications if we should
add_subdirectory(${GRAPHICS_SANDBOX_TESTS_ROOT})