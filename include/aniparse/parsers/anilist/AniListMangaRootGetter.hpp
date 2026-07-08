/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/manga/Manga.hpp"

namespace aniparse::parsers {

/**
 * @brief Root manga getter for AniList: search, latest, and URL routing over the
 * GraphQL Page/Media API. Stateless — one instance serves every request.
 */
class AniListMangaRootGetter : public MangaRootGetter {
public:
	NetworkRequestTask<SearchCompatibilities> search_support(RequestorContext context) override;

	MangaGetterRootCompatibilities latest_support() const noexcept override;

	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> search(
	    RequestorContext context,
	    SearchRequestQuery query,
	    GetFilters filters) override;

	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> latest(
	    RequestorContext context,
	    GetFilters filters) override;

	NetworkRequestTask<std::unique_ptr<MangaGetter>> parse_url(
	    RequestorContext context,
	    ParsedUrl url) override;

	NetworkRequestTask<std::unique_ptr<MangaGetter>> from_serialized(
	    SerializedGetterData data) override;
};

} // namespace aniparse::parsers
