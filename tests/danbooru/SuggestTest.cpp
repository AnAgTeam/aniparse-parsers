/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"
#include "support/ParserTest.hpp"
#include "danbooru/DanbooruChecks.hpp"

#include "aniparse/parsers/danbooru/DanbooruImagesGetter.hpp"
#include "aniparse/parsers/danbooru/DanbooruParser.hpp"

// Autocomplete tests for DanbooruImagesGetter::suggest, driven through the shared
// canned client (fixture by default, real API under ANIPARSE_TEST_LIVE). Exercises
// the request building (autocomplete endpoint, artist-kind narrowing) and the
// mapping of Danbooru's tag categories onto the search-key axes.

using namespace aniparse;
using aniparse::parsers::DanbooruImagesGetter;
using aniparse::parsertest::context_over;
using aniparse::parsertest::data_context;
using aniparse::parsertest::make_mock;
using aniparse::parsertest::read_fixture;

namespace {

std::shared_ptr<parsertest::CannedClientMock> suggest_mock() {
	return make_mock(read_fixture("danbooru/fixtures/autocomplete_cirno.json"));
}

} // namespace

CORO_TEST_CASE("suggest maps autocomplete entries onto search-key axes", "[danbooru]") {
	auto mock = suggest_mock();
	RequestorContext context = context_over(mock);

	DanbooruImagesGetter getter;
	auto result = co_await getter.suggest(context, "cir", std::nullopt);
	REQUIRE(result.has_value());
	REQUIRE(result->size() == 4);

	// value + label + popularity, and the tag category mapped to a search-key axis.
	CHECK((*result)[0].value == "cirno");
	CHECK((*result)[0].label == "cirno");
	REQUIRE((*result)[0].count.has_value());
	CHECK(*(*result)[0].count == 90000);
	REQUIRE((*result)[0].category.has_value());
	CHECK(*(*result)[0].category == search_keys::character);  // category 4

	CHECK((*result)[2].value == "cirnyaa");
	CHECK(*(*result)[2].category == search_keys::artist);     // category 1
	CHECK(*(*result)[3].category == search_keys::series);     // category 3 (copyright)
}

CORO_TEST_CASE("suggest builds the autocomplete request; artist kind narrows the type", "[danbooru]") {
	auto mock = suggest_mock();
	RequestorContext context = context_over(mock);

	DanbooruImagesGetter getter;
	auto result = co_await getter.suggest(context, "cir", std::string(search_keys::artist));
	REQUIRE(result.has_value());

	const auto* configured = std::get_if<ConfiguredGetRequest>(&mock->request);
	REQUIRE(configured != nullptr);
	const GetRequest& req = configured->request;

	CHECK(req.url.ends_with("/autocomplete.json"));
	CHECK(req.url_params.get("search[query]") == "cir");
	CHECK(req.url_params.get("search[type]") == "artist");   // artist axis -> dedicated type
}

CORO_TEST_CASE("suggest advertises the capability in search_support", "[danbooru]") {
	auto mock = suggest_mock();
	RequestorContext context = context_over(mock);

	DanbooruImagesGetter getter;
	auto support = co_await getter.search_support(context);
	REQUIRE(support.has_value());
	CHECK(support->compatibilities.has(compatibilities_flags::supports_suggestions));
	REQUIRE(support->supported_suggestion_kinds.size() == 1);
	CHECK(support->supported_suggestion_kinds.front() == search_keys::artist);
}

// One test, two data sources — the autocomplete fixture by default, the real
// danbooru.donmai.us endpoint under ANIPARSE_TEST_LIVE=1.
CORO_TEST_CASE("Danbooru: suggest shape (fixture by default, live under ANIPARSE_TEST_LIVE)", "[danbooru][live]") {
	parsers::DanbooruParser parser;
	RequestorContext context = data_context(parser, "danbooru/fixtures/autocomplete_cirno.json");
	co_await danbooru_checks::assert_suggest_shape(context, "cir");
}
