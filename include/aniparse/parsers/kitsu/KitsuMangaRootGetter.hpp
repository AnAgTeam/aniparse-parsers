/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/manga/Manga.hpp"

namespace aniparse::parsers {

/**
 * @brief Root manga getter for Kitsu: search, latest, and URL routing over the
 * JSON:API /manga collection. Stateless — one instance serves every request.
 */
class KitsuMangaRootGetter : public MangaRootGetter {
public:
	NetworkRequestTask<SearchCompatibilities> search_support(RequestorContext context) const override;

	MangaGetterRootCompatibilities latest_support() const noexcept override;

	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> search(
	    RequestorContext context,
	    SearchRequestQuery query,
	    GetFilters filters) const override;

	NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> latest(
	    RequestorContext context,
	    GetFilters filters) const override;

	NetworkRequestTask<std::unique_ptr<MangaGetter>> parse_url(
	    RequestorContext context,
	    ParsedUrl url) const override;

	NetworkRequestTask<std::unique_ptr<MangaGetter>> from_serialized(
	    SerializedGetterData data) const override;
};

} // namespace aniparse::parsers
