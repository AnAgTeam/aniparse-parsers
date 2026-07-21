/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/manga/Manga.hpp"

#include <optional>

namespace aniparse::parsers::demo {

struct DemoTitle;

/// Build the short-card info a listing hands to a getter (title, cover, rating,
/// tags). Shared by the root getter's search/latest so both cards look alike.
[[nodiscard]] MangaInfo demo_preview(const DemoTitle& title);

/**
 * @brief A single demo title: its metadata, its chapters and their (embedded) pages.
 * Serves the whole manga read path from the static catalogue — no request leaves
 * the process. @see DemoData
 */
class DemoMangaGetter : public MangaGetter {
public:
	explicit DemoMangaGetter(int id, std::optional<MangaInfo> preview = std::nullopt);

	MangaGetterCompatibilities compatibilities() const noexcept override;
	std::optional<MangaInfo> preview_info() const noexcept override;

	NetworkRequestTask<MangaInfo> info(RequestorContext context) const override;

	NetworkRequestTask<PageResults<MangaChapterInfo>> chapters_info(
	    RequestorContext context,
	    GetFilters filters,
	    std::optional<MangaTranslationID> translation = std::nullopt) const override;

	NetworkRequestTask<PageResults<MangaPage>> chapter_pages(
	    RequestorContext context,
	    MangaChapterRef chapter,
	    GetFilters filters,
	    std::optional<MangaTranslationID> translation = std::nullopt) const override;

	NetworkRequestTask<PageResults<RelatedWork>> related(
	    RequestorContext context,
	    GetFilters filters) const override;

	NetworkRequestTask<SerializedGetterData> serialize() const override;

private:
	int                      id_;
	std::optional<MangaInfo> preview_;
};

} // namespace aniparse::parsers::demo
