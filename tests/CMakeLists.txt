
# Deterministic parser tests: pure mapping over synthetic JSON fixtures plus
# whole-method runs driven through a canned client, no network and no live host.
# Built as its own target so the ASan / analyze legs can drive it. Each parser's
# tests and fixtures live under its own folder (danbooru/, ...); shared doubles and
# loaders live in support/.
add_executable(aniparse-parsers-tests
	"catch_amalgamated.cpp" "catch_amalgamated.hpp"
	"CoroTest.hpp"
	"support/ParserTest.hpp"
	"support/LiveClient.cpp"
	"danbooru/DanbooruChecks.hpp"
	"danbooru/MappingTest.cpp"
	"danbooru/SearchTest.cpp"
)

set_property(TARGET aniparse-parsers-tests PROPERTY CXX_STANDARD 23)
set_property(TARGET aniparse-parsers-tests PROPERTY CXX_STANDARD_REQUIRED ON)

target_link_libraries(aniparse-parsers-tests PRIVATE aniparse-parsers)

# The tests dir is on the include path so "CoroTest.hpp" / "support/ParserTest.hpp"
# resolve from a parser subfolder; fixtures are read at runtime by path relative to
# the same dir (e.g. "danbooru/fixtures/post_still.json").
target_include_directories(aniparse-parsers-tests PRIVATE "${CMAKE_CURRENT_LIST_DIR}")
target_compile_definitions(aniparse-parsers-tests PRIVATE
	ANIPARSE_PARSERS_TESTS_DIR="${CMAKE_CURRENT_LIST_DIR}")

add_test(NAME aniparse-parsers-tests COMMAND aniparse-parsers-tests)
