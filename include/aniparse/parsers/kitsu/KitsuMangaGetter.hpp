/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/manga/Manga.hpp"

#include <optional>
#include <string>

namespace aniparse::parsers {

/**
 * @brief One-manga getter for Kitsu. Identity is the URL ref — a numeric id or a
 * slug — round-tripped through serialize().
 *
 * info()/preview_info() are the implemented surface; chapters and pages fall to
 * the base NotImplemented defaults, as Kitsu hosts no chapter content.
 */
class KitsuMangaGetter : public MangaGetter {
public:
	/// @param ref     Numeric id ("38") or slug ("one-piece"); info() resolves
	///                either against the API.
	/// @param preview Short-card info from a search result, returned by
	///                preview_info without a network call; nullopt from a URL alone.
	explicit KitsuMangaGetter(std::string ref, std::optional<MangaInfo> preview = std::nullopt);

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
	std::string ref_;
	std::optional<MangaInfo> preview_;
};

} // namespace aniparse::parsers
