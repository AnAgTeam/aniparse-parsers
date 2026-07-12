/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/gelbooru/GelbooruParser.hpp"
#include "aniparse/parsers/gelbooru/GelbooruEngine.hpp"

namespace aniparse::parsers::gelbooru {

namespace {
	using namespace std::string_view_literals;

	constexpr std::string_view domains[]   = { "gelbooru.com"sv };
	constexpr std::string_view api_hosts[] = { "https://gelbooru.com"sv };

	// The Gelbooru site descriptor: identity, routing domains and API host, bound to
	// the shared Gelbooru engine. Credentialed — the DAPI is auth-walled, so user_id +
	// api_key ride as static url params; adult source.
	constexpr engines::BooruSite descriptor{
	    .identifier       = "Gelbooru",
	    .name             = "Gelbooru",
	    .primary_language = "en",
	    .domains          = domains,
	    .api_hosts        = api_hosts,
	    .media_referer    = "https://gelbooru.com/", // img host hotlink-protects media
	    .engine           = &engine,
	    .compatibilities  = compatibilities_flags::supports_images_search
	                      | compatibilities_flags::adult_source,
	    .auth             = engines::BooruStaticParamAuth{ .user_param = "user_id", .key_param = "api_key" },
	};
} // namespace

const engines::BooruSite& site() {
	return descriptor;
}

} // namespace aniparse::parsers::gelbooru

namespace aniparse::parsers {

GelbooruParser::GelbooruParser() : engines::BooruParser(gelbooru::site()) {}

} // namespace aniparse::parsers
