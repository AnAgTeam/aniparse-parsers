/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/kitsu/KitsuParser.hpp"
#include "aniparse/parsers/kitsu/KitsuMangaRootGetter.hpp"
#include "aniparse/parsers/kitsu/detail/KitsuApi.hpp"

namespace aniparse::parsers {

std::string KitsuParser::name() const {
	return "Kitsu";
}

std::string KitsuParser::identifier() const {
	return "Kitsu";
}

GetterSuggestionType KitsuParser::suggest_getter(const ParsedUrl& url) const {
	// Kitsu serves /anime/ and /manga/ under one host; this parser handles manga.
	return url.path().find("/manga/") != std::string_view::npos
	           ? GetterSuggestionType::Manga
	           : GetterSuggestionType::Unknown;
}

ParserCompatibilities KitsuParser::compatibilities() const {
	return {
	    .primary_language = "en",
	    .flags            = compatibilities_flags::supports_manga_store,
	};
}

void KitsuParser::emplace_domains(EmplaceDomainsContext& context) const {
	context.add_domain("kitsu.io");
	context.add_domain("kitsu.app");
}

void KitsuParser::configure(ParserConfig& config) const {
	for (const auto& [name, value] : kitsu::api_headers()) {
		config.headers.set(name, value);
	}
}

std::unique_ptr<MangaRootGetter> KitsuParser::mangas_getter() const {
	return std::make_unique<KitsuMangaRootGetter>();
}

} // namespace aniparse::parsers
