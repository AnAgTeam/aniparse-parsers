/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/manga/Manga.hpp"

#include <optional>

namespace aniparse::parsers {

/**
 * @brief One-manga getter for AniList. Identity is the numeric media id,
 * round-tripped through serialize().
 *
 * info() and preview_info() are the implemented surface; chapters and pages are
 * left to the base NotImplemented defaults, as AniList hosts no chapter content.
 */
class AniListMangaGetter : public MangaGetter {
public:
	/// @param preview Short-card info from a search/latest result, returned by
	/// preview_info without a network call; nullopt when built from a URL alone.
	explicit AniListMangaGetter(int media_id, std::optional<MangaInfo> preview = std::nullopt);

	MangaGetterCompatibilities compatibilities() const noexcept override;

	NetworkRequestTask<MangaInfo> preview_info(RequestorContext context) override;

	NetworkRequestTask<MangaInfo> info(RequestorContext context) override;

	NetworkRequestTask<PageResults<MangaPage>> chapter_pages(
	    RequestorContext context,
	    MangaChapterRef chapter,
	    GetFilters filters,
	    std::optional<MangaTranslationID> translation) override;

	NetworkRequestTask<SerializedGetterData> serialize() override;

private:
	int media_id_;
	std::optional<MangaInfo> preview_;
};

} // namespace aniparse::parsers
