/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"
#include "support/ParserTest.hpp"

#include "aniparse/parsers/gelbooru/GelbooruImagesGetter.hpp"

// Whole-method tests for Gelbooru search, driven through the shared canned client:
// search() -> list_request -> request_json -> posts_of/build_post_page -> mapping.
// Exercises the DAPI request building and the wrapped-envelope parsing that a bare
// array (Danbooru) never does.

using namespace aniparse;
using aniparse::parsers::GelbooruImagesGetter;
using aniparse::parsertest::context_over;
using aniparse::parsertest::make_mock;
using aniparse::parsertest::read_fixture;

namespace {

std::shared_ptr<parsertest::CannedClientMock> page_mock() {
	return make_mock(read_fixture("gelbooru/fixtures/posts_page.json"));
}

} // namespace

CORO_TEST_CASE("gelbooru search maps the DAPI envelope into container getters", "[gelbooru]") {
	auto mock = page_mock();
	RequestorContext context = context_over(mock);

	GelbooruImagesGetter getter;
	auto page = co_await getter.search(context, SearchRequestQuery{ .query = "cat_ears" }, GetFilters{});
	REQUIRE(page.has_value());

	REQUIRE(page->results.size() == 2);
	CHECK(page->results[0].offset == 0);
	CHECK(page->results[1].offset == 1);
	CHECK(page->next_offset == 2);

	// The card the listing mapped, off the constructor — not info(), which goes back to the
	// source. Against the canned mock that distinction was invisible here: Gelbooru's
	// single_post() takes posts[0] of whatever it is handed, so a detail request answered
	// with the LISTING still produced post 0 and this passed. It would have passed just as
	// happily for results[1], reading post 0's data.
	auto info0 = page->results[0].item->preview_info();
	REQUIRE(info0.has_value());
	CHECK(info0->id == 8000001);
	CHECK(info0->common.tags.size() == 6);

	auto items0 = co_await page->results[0].item->items(context, GetFilters{});
	REQUIRE(items0.has_value());
	REQUIRE(items0->results.size() == 1);
	CHECK(items0->results[0].item.kind == ImageItemKind::Still);
	CHECK(items0->results[0].item.image.headers.get("Referer") == "https://gelbooru.com/");

	auto items1 = co_await page->results[1].item->items(context, GetFilters{});
	REQUIRE(items1.has_value());
	REQUIRE(items1->results.size() == 1);
	CHECK(items1->results[0].item.kind == ImageItemKind::Video);
}

CORO_TEST_CASE("gelbooru search builds the DAPI post-index request", "[gelbooru]") {
	auto mock = page_mock();
	RequestorContext context = context_over(mock);

	GelbooruImagesGetter getter;
	auto page = co_await getter.search(context, SearchRequestQuery{ .query = "cat_ears" }, GetFilters{});
	REQUIRE(page.has_value());

	const auto* configured = std::get_if<ConfiguredGetRequest>(&mock->request);
	REQUIRE(configured != nullptr);
	const GetRequest& req = configured->request;

	CHECK(req.url.ends_with("/index.php"));
	CHECK(req.url_params.get("page") == "dapi");
	CHECK(req.url_params.get("s") == "post");
	CHECK(req.url_params.get("q") == "index");
	CHECK(req.url_params.get("json") == "1");
	CHECK(req.url_params.get("tags") == "cat_ears");
	CHECK(req.url_params.get("pid") == "0");    // from 0, 0-based page index
	CHECK(req.url_params.get("limit") == "20");
}

CORO_TEST_CASE("gelbooru search folds a supported sort into a sort: metatag", "[gelbooru]") {
	auto mock = page_mock();
	RequestorContext context = context_over(mock);

	GetFilters filters;
	filters.sort = SortOrder{ .key = std::string(sort_keys::popularity), .ascending = false };

	GelbooruImagesGetter getter;
	auto page = co_await getter.search(context, SearchRequestQuery{ .query = "cat_ears" }, filters);
	REQUIRE(page.has_value());

	const auto* configured = std::get_if<ConfiguredGetRequest>(&mock->request);
	REQUIRE(configured != nullptr);
	CHECK(configured->request.url_params.get("tags") == "cat_ears sort:score:desc");
}
