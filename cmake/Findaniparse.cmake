
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/libaniparse")

set(ANIPARSE_LIBRARIES aniparse)
set(ANIPARSE_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/libaniparse/include)
set(ANIPARSE_FOUND ON)

message(STATUS "Found aniparse libraries: ${ANIPARSE_LIBRARIES}")
message(STATUS "Found aniparse includes: ${ANIPARSE_INCLUDE_DIRS}")
