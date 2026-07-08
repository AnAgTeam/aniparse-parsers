cmake_minimum_required (VERSION 3.18)

# ---- PREPARE

option(ANIPARSE_PARSERS_BUILD_EXAMPLES "build examples, Default=ON" ON)

# without this vcpkg won't try to install dependencies
if (CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg.cmake")
	message(STATUS "Found vcpkg")
	include(${CMAKE_TOOLCHAIN_FILE})
endif()

project(aniparse-parsers VERSION 0.1
	DESCRIPTION "Canonical example parsers for libaniparse (public metadata APIs)"
	LANGUAGES CXX)

# ---- FIND DEPENDENCIES
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

find_package(aniparse REQUIRED)

# ---- ADD LIBRARY

add_library(aniparse-parsers STATIC
	"src/parsers/DefaultParsers.cpp" "include/aniparse/parsers/DefaultParsers.hpp"
	"src/parsers/anilist/detail/AniListApi.cpp" "include/aniparse/parsers/anilist/detail/AniListApi.hpp"
	"src/parsers/anilist/AniListParser.cpp" "include/aniparse/parsers/anilist/AniListParser.hpp"
	"src/parsers/anilist/AniListMangaRootGetter.cpp" "include/aniparse/parsers/anilist/AniListMangaRootGetter.hpp"
	"src/parsers/anilist/AniListMangaGetter.cpp" "include/aniparse/parsers/anilist/AniListMangaGetter.hpp"
	"src/parsers/kitsu/detail/KitsuApi.cpp" "include/aniparse/parsers/kitsu/detail/KitsuApi.hpp"
	"src/parsers/kitsu/KitsuParser.cpp" "include/aniparse/parsers/kitsu/KitsuParser.hpp"
	"src/parsers/kitsu/KitsuMangaRootGetter.cpp" "include/aniparse/parsers/kitsu/KitsuMangaRootGetter.hpp"
	"src/parsers/kitsu/KitsuMangaGetter.cpp" "include/aniparse/parsers/kitsu/KitsuMangaGetter.hpp"
)

set_property(TARGET aniparse-parsers PROPERTY CXX_STANDARD 20)
set_property(TARGET aniparse-parsers PROPERTY CXX_STANDARD_REQUIRED ON)

target_include_directories(aniparse-parsers PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_link_libraries(aniparse-parsers PUBLIC
	$<BUILD_INTERFACE:aniparse>
)

if (ANIPARSE_PARSERS_BUILD_EXAMPLES)
	add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/examples")
endif()

# ---- CONFIGURE PACKAGE

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

configure_package_config_file(
	"${CMAKE_CURRENT_LIST_DIR}/aniparse-parsers-config.cmake.in"
	"${CMAKE_CURRENT_BINARY_DIR}/aniparse-parsers-config.cmake"
	INSTALL_DESTINATION
		"${CMAKE_INSTALL_LIBDIR}/cmake/aniparse-parsers"
	NO_SET_AND_CHECK_MACRO
	NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

set(target1 aniparse-parsers)
configure_file("aniparse-parsers.pc.in" "aniparse-parsers.pc" @ONLY)

# always prefix with lib*
set_target_properties(aniparse-parsers PROPERTIES PREFIX "lib")

install(DIRECTORY "include/" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/aniparse-parsers.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

install(TARGETS aniparse-parsers
	EXPORT aniparse-parsers-targets
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(EXPORT aniparse-parsers-targets
	NAMESPACE aniparse::
	DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/aniparse-parsers"
	EXPORT_LINK_INTERFACE_LIBRARIES
)
