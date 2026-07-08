/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Parser.hpp"

namespace aniparse::parsers {

/**
 * @brief Danbooru (danbooru.donmai.us) — a public, tag-indexed image board.
 *
 * A showcase parser for the images domain, and the first with a real read-path:
 * unlike the metadata sources (AniList, Kitsu), Danbooru both catalogs and serves
 * media, driven by its documented REST API (`/posts.json`) that needs no auth to
 * read. Each post maps to a container-of-one; search is by tags.
 */
class DanbooruParser : public Parser {
public:
	std::string name() const override;
	std::string identifier() const override;
	GetterSuggestionType suggest_getter(const ParsedUrl& url) const override;
	ParserCompatibilities compatibilities() const override;
	void emplace_domains(EmplaceDomainsContext& context) const override;
	void configure(ParserConfig& config) const override;
	std::span<const std::string_view> mirrors() const override;
	std::unique_ptr<ImagesGetter> images_getter() const override;
};

} // namespace aniparse::parsers
