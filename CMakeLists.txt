cmake_minimum_required (VERSION 3.18)

# ---- PREPARE

option(ANIPARSE_PARSERS_BUILD_EXAMPLES "build examples, Default=ON" ON)
option(ANIPARSE_PARSERS_BUILD_TESTS "build the parser test suite, Default=ON" ON)

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
	"src/parsers/danbooru/detail/DanbooruApi.cpp" "include/aniparse/parsers/danbooru/detail/DanbooruApi.hpp"
	"src/parsers/danbooru/DanbooruParser.cpp" "include/aniparse/parsers/danbooru/DanbooruParser.hpp"
	"src/parsers/danbooru/DanbooruImagesGetter.cpp" "include/aniparse/parsers/danbooru/DanbooruImagesGetter.hpp"
	"src/parsers/danbooru/DanbooruContainerGetter.cpp" "include/aniparse/parsers/danbooru/DanbooruContainerGetter.hpp"
	"src/parsers/danbooru/DanbooruPoolGetter.cpp" "include/aniparse/parsers/danbooru/DanbooruPoolGetter.hpp"
	"src/parsers/gelbooru/detail/GelbooruApi.cpp" "include/aniparse/parsers/gelbooru/detail/GelbooruApi.hpp"
	"src/parsers/gelbooru/GelbooruParser.cpp" "include/aniparse/parsers/gelbooru/GelbooruParser.hpp"
	"src/parsers/gelbooru/GelbooruImagesGetter.cpp" "include/aniparse/parsers/gelbooru/GelbooruImagesGetter.hpp"
	"src/parsers/gelbooru/GelbooruContainerGetter.cpp" "include/aniparse/parsers/gelbooru/GelbooruContainerGetter.hpp"
)

set_property(TARGET aniparse-parsers PROPERTY CXX_STANDARD 20)
set_property(TARGET aniparse-parsers PROPERTY CXX_STANDARD_REQUIRED ON)

# Opt-in MSVC static lifetime analysis (a separate, slow CI leg — not per-build).
# The C++ Core Guidelines lifetime rules catch dangling views/pointers at compile
# time (e.g. `std::string_view v = json::str(...)` -> C26815/C26449). Scoped to
# our sources only (PRIVATE), system headers excluded, and the lifetime rules are
# promoted to errors so a regression fails the build; the noisier style rules stay
# non-fatal.
option(ANIPARSE_PARSERS_ANALYZE "MSVC /analyze lifetime gate (slow; CI leg)" OFF)
if (ANIPARSE_PARSERS_ANALYZE AND MSVC)
	target_compile_options(aniparse-parsers PRIVATE
		/analyze /analyze:external- /analyze:plugin EspXEngine.dll
		/external:env:INCLUDE /external:W0
		# Lifetime rules -> errors: a dangling view/pointer/use-after-move fails.
		/we26815 /we26816 /we26449 /we26800 /we26811
		# Silence the gsl-everywhere style profile (noexcept/const/gsl::at/span/
		# narrow_cast/bounds/switch) — opinionated, not correctness; keeps the gate
		# signal-only. 26818 (switch coverage) also fires inside vendored lexbor.
		/wd26440 /wd26446 /wd26447 /wd26455 /wd26439 /wd26432 /wd26818
		/wd26472 /wd26481 /wd26482 /wd26821 /wd26496 /wd26497 /wd26475 /wd26459)
endif()

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

if (ANIPARSE_PARSERS_BUILD_TESTS)
	enable_testing()
	add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/tests")
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
