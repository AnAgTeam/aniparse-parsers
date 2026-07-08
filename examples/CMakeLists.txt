
add_executable(aniparse-parsers-example
	"${CMAKE_CURRENT_LIST_DIR}/example.cpp"
)

set_property(TARGET aniparse-parsers-example PROPERTY CXX_STANDARD 23)
set_property(TARGET aniparse-parsers-example PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(aniparse-parsers-example PRIVATE aniparse-parsers)
