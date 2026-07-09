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

// Whole-method tests for the Danbooru search path, driven through a canned client
// so the REAL getter code runs offline: search() -> list_request -> request_json ->
// build_post_page -> container mapping. This is the tier that exercises the request
// building and the page-loop wiring (including a banned post mid-page), none of which
// the pure per-post mapping tests reach — those functions live in an anonymous
// namespace and are only callable through the public method. Runs under ASan.
// The client double and fixture loader are shared (support/ParserTest.hpp).

using namespace aniparse;
using aniparse::parsers::DanbooruImagesGetter;
using aniparse::parsertest::context_over;
using aniparse::parsertest::data_context;
using aniparse::parsertest::make_mock;
using aniparse::parsertest::read_fixture;

namespace {

std::shared_ptr<parsertest::CannedClientMock> page_mock() {
	return make_mock(read_fixture("danbooru/fixtures/posts_page.json"));
}

} // namespace

CORO_TEST_CASE("search maps a posts page into container getters") {
	auto mock = page_mock();
	RequestorContext context = context_over(mock);

	DanbooruImagesGetter getter;
	auto page = co_await getter.search(context, SearchRequestQuery{ .query = "cirno" }, GetFilters{});
	REQUIRE(page.has_value());

	// The page carries all three posts (still, video, banned) in order, each with
	// its absolute offset, and reports the next offset past them.
	REQUIRE(page->results.size() == 3);
	CHECK(page->results[0].offset == 0);
	CHECK(page->results[2].offset == 2);
	CHECK(page->next_offset == 3);

	// Post 0 (still): info + a single still leaf, no refetch (cached at build time).
	auto info0 = co_await page->results[0].item->info(context);
	REQUIRE(info0.has_value());
	CHECK(info0->id == 5000010);
	CHECK(info0->title == "cirno (touhou_project)");
	CHECK(info0->tags.size() == 9); // 1 artist + 1 copyright + 1 character + 1 meta + 5 general
	auto items0 = co_await page->results[0].item->items(context, GetFilters{});
	REQUIRE(items0.has_value());
	REQUIRE(items0->results.size() == 1);
	CHECK(items0->results[0].item.kind == ImageItemKind::Still);

	// Post 1 (webm): a single video leaf with a poster.
	auto items1 = co_await page->results[1].item->items(context, GetFilters{});
	REQUIRE(items1.has_value());
	REQUIRE(items1->results.size() == 1);
	CHECK(items1->results[0].item.kind == ImageItemKind::Video);
	CHECK(items1->results[0].item.poster.has_value());

	// Post 2 (banned): metadata survives, but the null file_url yields no media leaf
	// — the discriminating case that only shows up when the page loop runs for real.
	auto info2 = co_await page->results[2].item->info(context);
	REQUIRE(info2.has_value());
	CHECK(info2->id == 5000012);
	CHECK(info2->title == "cirno (original)");
	auto items2 = co_await page->results[2].item->items(context, GetFilters{});
	REQUIRE(items2.has_value());
	CHECK(items2->results.empty());
}

// One test, two data sources: the fixture by default (deterministic, runs in CI) and
// the real danbooru.donmai.us under ANIPARSE_TEST_LIVE=1 — same assertions, no second
// copy of the test and nothing backend-specific here.
CORO_TEST_CASE("Danbooru: search shape (fixture by default, live under ANIPARSE_TEST_LIVE)", "[live]") {
	parsers::DanbooruParser parser;
	RequestorContext context = data_context(parser, "danbooru/fixtures/posts_page.json");
	co_await danbooru_checks::assert_search_shape(context, "cirno rating:general");
}

CORO_TEST_CASE("search builds the /posts.json request with tags, page and limit") {
	auto mock = page_mock();
	RequestorContext context = context_over(mock);

	DanbooruImagesGetter getter;
	auto page = co_await getter.search(context, SearchRequestQuery{ .query = "cirno" }, GetFilters{});
	REQUIRE(page.has_value());

	const auto* configured = std::get_if<ConfiguredGetRequest>(&mock->request);
	REQUIRE(configured != nullptr);
	const GetRequest& req = configured->request;

	CHECK(req.url.ends_with("/posts.json"));
	CHECK(req.url_params.get("tags") == "cirno");
	CHECK(req.url_params.get("page") == "1");   // from 0, default limit 20 -> page 1
	CHECK(req.url_params.get("limit") == "20");
}

CORO_TEST_CASE("search folds a supported sort into an order: metatag") {
	auto mock = page_mock();
	RequestorContext context = context_over(mock);

	GetFilters filters;
	filters.sort = SortOrder{ .key = std::string(sort_keys::popularity), .ascending = false };

	DanbooruImagesGetter getter;
	auto page = co_await getter.search(context, SearchRequestQuery{ .query = "cirno" }, filters);
	REQUIRE(page.has_value());

	const auto* configured = std::get_if<ConfiguredGetRequest>(&mock->request);
	REQUIRE(configured != nullptr);
	// The sort rides inside the tag string as an order: metatag (exempt from the
	// anonymous 2-tag limit), not as a separate parameter.
	CHECK(configured->request.url_params.get("tags") == "cirno order:score");
}
