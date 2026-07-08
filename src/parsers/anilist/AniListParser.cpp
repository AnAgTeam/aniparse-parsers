/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/anilist/AniListParser.hpp"
#include "aniparse/parsers/anilist/AniListMangaRootGetter.hpp"
#include "aniparse/parsers/anilist/detail/AniListApi.hpp"

namespace aniparse::parsers {

std::string AniListParser::name() const {
	return "AniList";
}

std::string AniListParser::identifier() const {
	return "AniList";
}

GetterSuggestionType AniListParser::suggest_getter(const ParsedUrl& url) const {
	// AniList serves both /anime/ and /manga/ under one host; this parser handles
	// the manga catalog, so only manga URLs are ours.
	return url.path().find("/manga/") != std::string_view::npos
	           ? GetterSuggestionType::Manga
	           : GetterSuggestionType::Unknown;
}

ParserCompatibilities AniListParser::compatibilities() const {
	return {
	    .primary_language = "en",
	    .flags            = compatibilities_flags::supports_manga_store,
	};
}

void AniListParser::emplace_domains(EmplaceDomainsContext& context) const {
	context.add_domain("anilist.co");
}

void AniListParser::configure(ParserConfig& config) const {
	// The GraphQL headers are constant; baking them into the config means every
	// request through the readied context carries them without per-call repeat.
	for (const auto& [name, value] : anilist::api_headers()) {
		config.headers.set(name, value);
	}
}

std::span<const std::string_view> AniListParser::mirrors() const {
	// The GraphQL host, consumed by the getters via get_api_base() ->
	// RequestorContext::base_url, so a live catalog override can refresh it.
	return anilist::api_hosts();
}

std::unique_ptr<MangaRootGetter> AniListParser::mangas_getter() const {
	return std::make_unique<AniListMangaRootGetter>();
}

} // namespace aniparse::parsers
