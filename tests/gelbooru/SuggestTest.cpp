/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"
#include "support/ParserTest.hpp"
#include "gelbooru/GelbooruChecks.hpp"

#include "aniparse/parsers/gelbooru/GelbooruImagesGetter.hpp"
#include "aniparse/parsers/gelbooru/GelbooruParser.hpp"

// Autocomplete tests for GelbooruImagesGetter::suggest over the credential-free
// autocomplete2 endpoint. Exercises the string post_count parsing and category
// mapping. The live variant works without an api_key (unlike search).

using namespace aniparse;
using aniparse::parsers::GelbooruImagesGetter;
using aniparse::parsertest::context_over;
using aniparse::parsertest::data_context;
using aniparse::parsertest::make_mock;
using aniparse::parsertest::read_fixture;

namespace {

std::shared_ptr<parsertest::CannedClientMock> suggest_mock() {
	return make_mock(read_fixture("gelbooru/fixtures/autocomplete_cat.json"));
}

} // namespace

CORO_TEST_CASE("gelbooru suggest maps autocomplete2 rows onto search-key axes", "[gelbooru]") {
	auto mock = suggest_mock();
	RequestorContext context = context_over(mock);

	GelbooruImagesGetter getter;
	auto result = co_await getter.suggest(context, "cat", std::nullopt);
	REQUIRE(result.has_value());
	REQUIRE(result->size() == 3);

	CHECK((*result)[0].value == "cat_ears");
	CHECK((*result)[0].label == "cat_ears");
	REQUIRE((*result)[0].count.has_value());
	CHECK(*(*result)[0].count == 418575);       // post_count arrives as a STRING
	CHECK_FALSE((*result)[0].category.has_value()); // "tag" -> untyped

	CHECK(*(*result)[1].category == search_keys::character);
	CHECK(*(*result)[2].category == search_keys::artist);
}

CORO_TEST_CASE("gelbooru suggest builds the autocomplete2 request", "[gelbooru]") {
	auto mock = suggest_mock();
	RequestorContext context = context_over(mock);

	GelbooruImagesGetter getter;
	auto result = co_await getter.suggest(context, "cat", std::nullopt);
	REQUIRE(result.has_value());

	const auto* configured = std::get_if<ConfiguredGetRequest>(&mock->request);
	REQUIRE(configured != nullptr);
	const GetRequest& req = configured->request;
	CHECK(req.url.ends_with("/index.php"));
	CHECK(req.url_params.get("page") == "autocomplete2");
	CHECK(req.url_params.get("term") == "cat");
	CHECK(req.url_params.get("type") == "tag_query");
}

CORO_TEST_CASE("gelbooru advertises suggestions in search_support", "[gelbooru]") {
	auto mock = suggest_mock();
	RequestorContext context = context_over(mock);

	GelbooruImagesGetter getter;
	auto support = co_await getter.search_support(context);
	REQUIRE(support.has_value());
	CHECK(support->compatibilities.has(compatibilities_flags::supports_suggestions));
}

// autocomplete2 needs no credentials, so this runs live under ANIPARSE_TEST_LIVE
// (search cannot — the DAPI is auth-walled).
CORO_TEST_CASE("Gelbooru: suggest shape (fixture by default, live under ANIPARSE_TEST_LIVE)", "[gelbooru][live]") {
	parsers::GelbooruParser parser;
	RequestorContext context = data_context(parser, "gelbooru/fixtures/autocomplete_cat.json");
	co_await gelbooru_checks::assert_suggest_shape(context, "cat");
}
