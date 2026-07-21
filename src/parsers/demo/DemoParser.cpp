/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/demo/DemoParser.hpp"
#include "aniparse/parsers/demo/DemoMangaRootGetter.hpp"

namespace aniparse::parsers::demo {

ParserInfo DemoParser::info() const {
	return {
	    .name             = "Асагао (демо)",
	    .primary_language = "ru",
	};
}

std::string DemoParser::identifier() const {
	return "Demo";
}

ParserCompatibilities DemoParser::compatibilities() const {
	return {
	    .flags = compatibilities_flags::supports_manga_store   // a browsable manga library
	           | compatibilities_flags::supports_reading       // that hosts its own pages
	           | compatibilities_flags::supports_suggestions,  // and completes search tokens
	};
}

void DemoParser::emplace_domains(EmplaceDomainsContext& context) const {
	context.add_domain("asagao.demo");
}

GetterSuggestionType DemoParser::suggest_getter(const ParsedUrl& /*url*/) const {
	// Every URL under the demo domain addresses a manga — it is the only category
	// this source has.
	return GetterSuggestionType::Manga;
}

std::unique_ptr<MangaRootGetter> DemoParser::mangas_getter() const {
	return std::make_unique<DemoMangaRootGetter>();
}

} // namespace aniparse::parsers::demo
