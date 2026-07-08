/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Parser.hpp"

namespace aniparse::parsers {

/**
 * @brief AniList (anilist.co) — a public GraphQL metadata source.
 *
 * A Tier-1 showcase parser: search and info map cleanly onto the manga model,
 * driven by AniList's open GraphQL API (no auth for reads). AniList does not
 * host chapter content, so the reading path (chapters / pages) stays
 * unimplemented — this is a metadata parser by design.
 */
class AniListParser : public Parser {
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
