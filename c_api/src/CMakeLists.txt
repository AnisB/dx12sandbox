cmake_minimum_required(VERSION 3.2)

# The defines we need for the library
set(GRAPHICS_SANDBOX_SDK_INCLUDE ${GRAPHICS_SANDBOX_SDK_ROOT}/include)

bento_headers(header_files "${GRAPHICS_SANDBOX_CAPI_INCLUDE}" "c_api")
bento_sources(source_files "${GRAPHICS_SANDBOX_CAPI_SRC}" "c_api")

# Generate the static library
bento_dynamic_lib("graphics_sandbox_dylib" "c_api" "${header_files};${source_files};" "${GRAPHICS_SANDBOX_SDK_INCLUDE};${GRAPHICS_SANDBOX_CAPI_INCLUDE}")
target_link_libraries("graphics_sandbox_dylib" "graphics_sandbox_sdk" "bento_sdk" "${D3D12_LIBRARIES}")