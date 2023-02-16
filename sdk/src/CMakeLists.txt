cmake_minimum_required(VERSION 3.2)

# The defines we need for the library
set(GRAPHICS_SANDBOX_SDK_INCLUDE ${GRAPHICS_SANDBOX_SDK_ROOT}/include)
set(GRAPHICS_SANDBOX_SDK_SOURCE ${GRAPHICS_SANDBOX_SDK_ROOT}/src)

sub_directory_list(sub_projects_headers "${GRAPHICS_SANDBOX_SDK_INCLUDE}")
foreach(header_dir ${sub_projects_headers})
	bento_headers(tmp_header_list "${GRAPHICS_SANDBOX_SDK_INCLUDE}/${header_dir}" "${header_dir}")
	list(APPEND header_files "${tmp_header_list}")
endforeach()

sub_directory_list(sub_projects_sources "${GRAPHICS_SANDBOX_SDK_SOURCE}")
foreach(source_dir ${sub_projects_sources})
	bento_sources(tmp_source_list "${GRAPHICS_SANDBOX_SDK_SOURCE}/${source_dir}" "${source_dir}")
	list(APPEND source_files "${tmp_source_list}")
endforeach()

# Generate the static library
bento_static_lib("graphics_sandbox_sdk" "sdk" "${header_files};${source_files};" "${GRAPHICS_SANDBOX_SDK_INCLUDE};${GRAPHICS_SANDBOX_3RD_INCLUDES};${BENTO_SDK_ROOT}/include;${D3D12_INCLUDE_DIRS}")