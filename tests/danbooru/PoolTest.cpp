/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"
#include "support/ParserTest.hpp"

#include "aniparse/parsers/danbooru/DanbooruPoolGetter.hpp"

// Container-of-many tests: a Danbooru pool exercises the items() pagination path
// that a single-post container-of-one never touches. Driven through the shared
// canned client over fixtures (a pool metadata doc for info(); a posts page for
// items(), which — like any listing — is a bare post array).

using namespace aniparse;
using aniparse::parsers::DanbooruPoolGetter;
using aniparse::parsertest::context_over;
using aniparse::parsertest::make_mock;
using aniparse::parsertest::read_fixture;

CORO_TEST_CASE("pool info maps name, size and description", "[danbooru]") {
	auto mock = make_mock(read_fixture("danbooru/fixtures/pool_touhou.json"));
	RequestorContext context = context_over(mock);

	DanbooruPoolGetter getter(1234);
	auto info = co_await getter.info(context);
	REQUIRE(info.has_value());

	CHECK(info->id == 1234);
	CHECK(info->common.title == "Touhou_-_Cirno_Collection"); // the pool's own name, not synthesized
	REQUIRE(info->total_items.has_value());
	CHECK(*info->total_items == 3);                     // stated up front by the pool
	CHECK(info->common.description.text.starts_with("A synthetic pool"));
	CHECK(info->common.revision == "2026-02-10T12:00:00.000-05:00");
}

CORO_TEST_CASE("pool items page through the posts, skipping banned", "[danbooru]") {
	auto mock = make_mock(read_fixture("danbooru/fixtures/posts_page.json"));
	RequestorContext context = context_over(mock);

	DanbooruPoolGetter getter(1234);
	auto page = co_await getter.items(context, GetFilters{ .from = 0, .limit = 20 });
	REQUIRE(page.has_value());

	// Three posts in the page; the banned one has no servable file -> two media items,
	// each carrying its absolute offset, with next_offset past them.
	REQUIRE(page->results.size() == 2);
	CHECK(page->results[0].offset == 0);
	CHECK(page->results[0].item.kind == ImageItemKind::Still);
	CHECK(page->results[1].offset == 1);
	CHECK(page->results[1].item.kind == ImageItemKind::Video);
	CHECK(page->next_offset == 2);
}

CORO_TEST_CASE("pool items build the ordpool request with paging", "[danbooru]") {
	auto mock = make_mock(read_fixture("danbooru/fixtures/posts_page.json"));
	RequestorContext context = context_over(mock);

	DanbooruPoolGetter getter(1234);
	auto page = co_await getter.items(context, GetFilters{ .from = 40, .limit = 20 });
	REQUIRE(page.has_value());

	const auto* configured = std::get_if<ConfiguredGetRequest>(&mock->request);
	REQUIRE(configured != nullptr);
	const GetRequest& req = configured->request;

	CHECK(req.url.ends_with("/posts.json"));
	CHECK(req.url_params.get("tags") == "ordpool:1234"); // pool order via the metatag
	CHECK(req.url_params.get("page") == "3");            // from 40 / limit 20 + 1
	CHECK(req.url_params.get("limit") == "20");
	// Offsets are absolute from `from`, not page-local.
	CHECK(page->results[0].offset == 40);
}
