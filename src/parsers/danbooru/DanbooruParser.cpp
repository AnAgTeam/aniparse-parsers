/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/danbooru/DanbooruParser.hpp"
#include "aniparse/parsers/danbooru/DanbooruImagesGetter.hpp"
#include "aniparse/parsers/danbooru/detail/DanbooruApi.hpp"

namespace aniparse::parsers {

std::string DanbooruParser::name() const {
	return "Danbooru";
}

std::string DanbooruParser::identifier() const {
	return "Danbooru";
}

GetterSuggestionType DanbooruParser::suggest_getter(const ParsedUrl& url) const {
	// Danbooru addresses a single post at /posts/{id} and an ordered set at
	// /pools/{id}; both are image containers routed to the images getter.
	const std::string_view path = url.path();
	const bool is_container = path.find("/posts/") != std::string_view::npos
	                       || path.find("/pools/") != std::string_view::npos;
	return is_container ? GetterSuggestionType::Images : GetterSuggestionType::Unknown;
}

ParserCompatibilities DanbooruParser::compatibilities() const {
	// A searchable image source that also hosts adult-rated content (declared
	// honestly; the read-path itself defaults to nothing and callers pass their
	// own rating filter).
	return {
	    .primary_language = "en",
	    .flags            = compatibilities_flags::supports_images_search
	                      | compatibilities_flags::adult_source,
	};
}

void DanbooruParser::emplace_domains(EmplaceDomainsContext& context) const {
	context.add_domain("danbooru.donmai.us");
}

void DanbooruParser::configure(ParserConfig& config) const {
	for (const auto& [name, value] : danbooru::api_headers()) {
		config.headers.set(name, value);
	}
}

std::span<const std::string_view> DanbooruParser::mirrors() const {
	return danbooru::api_hosts();
}

std::unique_ptr<ImagesGetter> DanbooruParser::images_getter() const {
	return std::make_unique<DanbooruImagesGetter>();
}

} // namespace aniparse::parsers
