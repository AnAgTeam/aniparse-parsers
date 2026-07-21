/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Parser.hpp"

namespace aniparse::parsers::demo {

/**
 * @brief A built-in demo manga source with a small catalogue of original,
 * fictional titles and their artwork embedded in the library.
 *
 * It exists so the public build is a complete, usable product on its own —
 * a working library, source browsing and a reader without any external source —
 * and so the reading path can be exercised offline in tests and screenshots. It
 * fetches nothing: its getters answer from the static catalogue, and its images
 * are served from embedded bytes by the demo asset client. @see DemoData,
 * @see DemoAssetClient
 */
class DemoParser : public Parser {
public:
	ParserInfo info() const override;
	std::string identifier() const override;
	ParserCompatibilities compatibilities() const override;
	void emplace_domains(EmplaceDomainsContext& context) const override;
	GetterSuggestionType suggest_getter(const ParsedUrl& url) const override;
	std::unique_ptr<MangaRootGetter> mangas_getter() const override;
};

} // namespace aniparse::parsers::demo
