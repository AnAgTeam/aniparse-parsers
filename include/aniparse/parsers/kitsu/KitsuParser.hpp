/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Parser.hpp"

namespace aniparse::parsers {

/**
 * @brief Kitsu (kitsu.io) — a public JSON:API metadata source.
 *
 * A showcase parser alongside AniList, exercising the library's JSON:API
 * shape (offset pagination, sideloaded relationships). Metadata only: Kitsu
 * hosts no chapter content, so the reading path stays unimplemented.
 */
class KitsuParser : public Parser {
public:
	std::string name() const override;
	std::string identifier() const override;
	GetterSuggestionType suggest_getter(const ParsedUrl& url) const override;
	ParserCompatibilities compatibilities() const override;
	void emplace_domains(EmplaceDomainsContext& context) const override;
	void configure(ParserConfig& config) const override;
	std::span<const std::string_view> mirrors() const override;
	std::unique_ptr<MangaRootGetter> mangas_getter() const override;
};

} // namespace aniparse::parsers
