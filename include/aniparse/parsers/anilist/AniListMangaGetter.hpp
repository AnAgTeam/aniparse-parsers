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

	[[nodiscard]] std::optional<MangaInfo> preview_info() const noexcept override;

	NetworkRequestTask<MangaInfo> info(RequestorContext context) const override;

	/// The works AniList declares related to this one — including the anime adaptation,
	/// which this parser cannot open and hands over as ids instead. @see RelatedWork
	NetworkRequestTask<PageResults<RelatedWork>> related(
	    RequestorContext context,
	    GetFilters filters) const override;

	NetworkRequestTask<PageResults<MangaPage>> chapter_pages(
	    RequestorContext context,
	    MangaChapterRef chapter,
	    GetFilters filters,
	    std::optional<MangaTranslationID> translation) const override;

	NetworkRequestTask<SerializedGetterData> serialize() const override;

private:
	int media_id_;
	std::optional<MangaInfo> preview_;
};

} // namespace aniparse::parsers
