cmake_minimum_required(VERSION 2.8.7)
project(ECWolf)

if(POLICY CMP0054)
	cmake_policy(SET CMP0054 NEW)
endif()

option(ANDROID "Build for Android" OFF)

# Support cross compiling
option( FORCE_CROSSCOMPILE "Turn on cross compiling." NO )
if( FORCE_CROSSCOMPILE )
	set( CMAKE_CROSSCOMPILING TRUE )
endif()

if(CMAKE_CROSSCOMPILING)
	set(IMPORT_EXECUTABLES "IMPORTFILE-NOTFOUND" CACHE FILEPATH "Export file from native build.")
	include(${IMPORT_EXECUTABLES})
endif()

# Simplify pk3 building, add_pk3(filename srcdirectory)
function( add_pk3 PK3_NAME PK3_DIR )
	# Generate target name. Just use "pk3" for main pk3 target.
	string( REPLACE "." "_" PK3_TARGET ${PK3_NAME} )
	if( ${PK3_TARGET} STREQUAL "ecwolf_pk3" )
		set( PK3_TARGET "pk3" )
	endif()

	add_custom_command( OUTPUT ${OUTPUT_DIR}/${PK3_NAME}
		COMMAND zipdir -udf ${OUTPUT_DIR}/${PK3_NAME} ${PK3_DIR}
		COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OUTPUT_DIR}/${PK3_NAME} $<TARGET_FILE_DIR:ecwolf>/${PK3_NAME}
		DEPENDS zipdir )

	# Touch the zipdir executable here so that the pk3s are forced to rebuild
	# each time since their dependecy has "changed."
	if( NOT ANDROID )
		add_custom_target( ${PK3_TARGET} ALL
			COMMAND ${CMAKE_COMMAND} -E touch $<TARGET_FILE:zipdir>
			DEPENDS ${OUTPUT_DIR}/${PK3_NAME} )
	else()
		# Prepare assets directory for APK
		add_custom_target( ${PK3_TARGET} ALL
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${OUTPUT_DIR}/${PK3_NAME} ${OUTPUT_DIR}/assets/${PK3_NAME}
			COMMAND ${CMAKE_COMMAND} -E touch $<TARGET_FILE:zipdir>
			DEPENDS ${OUTPUT_DIR}/${PK3_NAME} )
	endif()
endfunction()

set(OUTPUT_DIR ${CMAKE_BINARY_DIR} CACHE PATH "Directory in which to build ECWolf.")

set(LZMA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/lzma/C")

option(GPL "Build GPL edition" ON)
option(USE_LIBTEXTSCREEN "Use libtextscreen instead of console iwad picker." OFF)

option(INTERNAL_ZLIB "Force build with internal zlib" OFF)
option(INTERNAL_BZIP2 "Force build with internal bzip2" OFF)
option(INTERNAL_JPEG "Force build with internal libjpeg" OFF)

find_package(ZLIB)
find_package(BZip2)
find_package(JPEG)

if(NOT ZLIB_FOUND OR INTERNAL_ZLIB)
	set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/zlib)
	add_subdirectory(zlib)
	set(ZLIB_LIBRARIES z)
	set(ZLIB_LIBRARY z)
endif()

if(NOT BZIP2_FOUND OR INTERNAL_BZIP2)
	set(BZIP2_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/bzip2)
	add_subdirectory(bzip2)
	set(BZIP2_LIBRARIES bz2)
endif()

if(NOT JPEG_FOUND OR INTERNAL_JPEG)
	set(JPEG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/jpeg-6b)
	add_subdirectory(jpeg-6b)
	set(JPEG_LIBRARIES jpeg)
	set(JPEG_LIBRARY jpeg)
endif()

if(MSVC)
	add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE)
endif()

option(BUILD_PATCHUTIL "Builds the Wolf patching utility." OFF)
if(BUILD_PATCHUTIL)
	add_subdirectory(tools/patchutil)
endif()

add_subdirectory(tools/zipdir)
add_subdirectory(tools/updaterevision)
add_subdirectory(wadsrc)

add_subdirectory(gdtoa)
add_subdirectory(lzma)
if(GPL AND USE_LIBTEXTSCREEN)
	add_subdirectory(textscreen)
endif()

if(ANDROID)
	set(LIBPNG_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android-libs)
	set(LIBPNG_LIBRARY png16_static)
	set(SIGCPP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android-libs/sigc++)
	set(SIGCPP_LIBRARY sigc++)
	set(TINYXML_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android-libs/TinyXML)
	set(TINYXML_LIBRARY TinyXML)
	set(TOUCHCONTROLS_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android-libs/TouchControls)
	set(TOUCHCONTROLS_LIBRARY touchcontrols)

	add_subdirectory(android-libs/libpng)
	add_subdirectory(android-libs/sigc++)
	add_subdirectory(android-libs/TinyXML)
	add_subdirectory(android-libs/TouchControls)
	add_subdirectory(android-libs/launcher)
endif()

add_subdirectory(src)

if( NOT CMAKE_CROSSCOMPILING )
	export(TARGETS ${CROSS_EXPORTS} FILE "${CMAKE_BINARY_DIR}/ImportExecutables.cmake" )
endif()
