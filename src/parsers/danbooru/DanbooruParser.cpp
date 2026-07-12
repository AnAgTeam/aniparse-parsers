/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/danbooru/DanbooruParser.hpp"
#include "aniparse/parsers/danbooru/DanbooruEngine.hpp"

namespace aniparse::parsers::danbooru {

namespace {
	using namespace std::string_view_literals;

	constexpr std::string_view domains[]   = { "danbooru.donmai.us"sv };
	constexpr std::string_view api_hosts[] = { "https://danbooru.donmai.us"sv };

	// The Danbooru site descriptor: identity, routing domains and API host, bound to
	// the shared Danbooru engine. Anonymous (reads need no credentials); adult source.
	constexpr engines::BooruSite descriptor{
	    .identifier       = "Danbooru",
	    .name             = "Danbooru",
	    .primary_language = "en",
	    .domains          = domains,
	    .api_hosts        = api_hosts,
	    .media_referer    = std::nullopt, // CDN serves media to a bare GET
	    .engine           = &engine,
	    .compatibilities  = compatibilities_flags::supports_images_search
	                      | compatibilities_flags::adult_source,
	    .auth             = std::nullopt,
	};
} // namespace

const engines::BooruSite& site() {
	return descriptor;
}

} // namespace aniparse::parsers::danbooru

namespace aniparse::parsers {

DanbooruParser::DanbooruParser() : engines::BooruParser(danbooru::site()) {}

} // namespace aniparse::parsers
