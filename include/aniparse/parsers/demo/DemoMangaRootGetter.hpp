/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/manga/Manga.hpp"

namespace aniparse::parsers::demo {

/**
 * @brief The root of the demo manga library: browse, search and open by url or
 * serialized identity, all over the static catalogue. @see DemoData
 */
class DemoMangaRootGetter : public MangaRootGetter {
public:
	NetworkRequestTask<SearchCompatibilities> search_support(RequestorContext context) const override;

	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> latest(
	    RequestorContext context,
	    GetFilters filters) const override;

	MangaGetterRootCompatibilities latest_support() const noexcept override;

	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> search(
	    RequestorContext context,
	    SearchRequestQuery query,
	    GetFilters filters) const override;

	NetworkRequestTask<std::vector<SearchSuggestion>> suggest(
	    RequestorContext context,
	    std::string partial,
	    std::optional<std::string> kind = std::nullopt) const override;

	NetworkRequestTask<std::unique_ptr<MangaGetter>> from_serialized(
	    SerializedGetterData data) const override;
};

} // namespace aniparse::parsers::demo
