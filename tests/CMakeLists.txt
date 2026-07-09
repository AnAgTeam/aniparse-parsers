
# Deterministic parser tests: pure mapping over synthetic JSON fixtures, no network
# and no live host. Built as its own target so the ASan / analyze legs can drive it.
add_executable(aniparse-parsers-tests
	"catch_amalgamated.cpp" "catch_amalgamated.hpp"
	"DanbooruMappingTest.cpp"
)

set_property(TARGET aniparse-parsers-tests PROPERTY CXX_STANDARD 23)
set_property(TARGET aniparse-parsers-tests PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(aniparse-parsers-tests PRIVATE aniparse-parsers)

# The fixtures live next to this file; the loader reads them at runtime by path.
target_compile_definitions(aniparse-parsers-tests PRIVATE
	ANIPARSE_PARSERS_TEST_FIXTURES_DIR="${CMAKE_CURRENT_LIST_DIR}/fixtures")

add_test(NAME aniparse-parsers-tests COMMAND aniparse-parsers-tests)
