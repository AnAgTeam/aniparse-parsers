/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "catch_amalgamated.hpp"

#include "aniparse/parsers/gelbooru/GelbooruImagesGetter.hpp"

#include <coro/task.hpp>

#include <string>
#include <utility>

/**
 * @file
 * Structural invariants of Gelbooru autocomplete shared by the fixture and live
 * suggest tests. autocomplete2 is Gelbooru's one credential-free surface, so unlike
 * search it can run live under ANIPARSE_TEST_LIVE without an api_key.
 */
namespace gelbooru_checks {

inline coro::task<void> assert_suggest_shape(aniparse::RequestorContext context, std::string partial) {
	using namespace aniparse;

	parsers::GelbooruImagesGetter getter;
	auto result = co_await getter.suggest(context, std::move(partial), std::nullopt);
	REQUIRE(result.has_value());
	CHECK_FALSE(result->empty());
	for (const auto& suggestion : *result) {
		CHECK_FALSE(suggestion.value.empty());
		CHECK_FALSE(suggestion.label.empty());
	}
}

} // namespace gelbooru_checks
