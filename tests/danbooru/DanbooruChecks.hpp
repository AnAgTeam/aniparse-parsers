/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "catch_amalgamated.hpp"

#include "aniparse/parsers/danbooru/DanbooruImagesGetter.hpp"

#include <coro/task.hpp>

#include <string>
#include <utility>

/**
 * @file
 * The structural invariants of a Danbooru search that hold for BOTH a fixture-backed
 * context and the live API — the shared "logic" the two data sources switch under.
 * The fixture test drives it through a canned client and adds exact-value checks; the
 * live test drives the same assertions against the real host. Deliberately loose (no
 * exact counts or URLs) so it survives real, changing data.
 */
namespace danbooru_checks {

inline coro::task<void> assert_search_shape(aniparse::RequestorContext context, std::string tags) {
	using namespace aniparse;

	parsers::DanbooruImagesGetter getter;
	SearchRequestQuery query;
	query.query = std::move(tags);

	auto page = co_await getter.search(context, query, GetFilters{ .from = 0, .limit = 3 });
	REQUIRE(page.has_value());
	CHECK_FALSE(page->results.empty());

	for (const auto& entry : page->results) {
		auto info = co_await entry.item->info(context);
		REQUIRE(info.has_value());
		CHECK_FALSE(info->title.empty());
		CHECK_FALSE(info->tags.empty());
	}

	if (!page->results.empty()) {
		auto media_page = co_await page->results.front().item->items(context, GetFilters{});
		REQUIRE(media_page.has_value());
		for (const auto& page_item : media_page->results) {
			CHECK(page_item.item.image.url.starts_with("http"));
		}
	}
}

// Structural invariants of Danbooru autocomplete, shared by the fixture and live
// suggest tests: some suggestions come back, and each carries an insertable token.
inline coro::task<void> assert_suggest_shape(aniparse::RequestorContext context, std::string partial) {
	using namespace aniparse;

	parsers::DanbooruImagesGetter getter;
	auto result = co_await getter.suggest(context, std::move(partial), std::nullopt);
	REQUIRE(result.has_value());
	CHECK_FALSE(result->empty());
	for (const auto& suggestion : *result) {
		CHECK_FALSE(suggestion.value.empty());
		CHECK_FALSE(suggestion.label.empty());
	}
}

} // namespace danbooru_checks
