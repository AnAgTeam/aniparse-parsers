/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <string>
#include <string_view>
#include <vector>

/*
 * The demo source's static catalogue — a handful of original, fictional titles.
 * Nothing here is a real work: the titles, blurbs and artwork are invented so the
 * demo source is a complete, self-contained, non-infringing reading experience the
 * public build can ship. @see DemoParser
 */

namespace aniparse::parsers::demo {

/// The URL scheme the demo parser stamps onto every image it produces. The demo
/// asset client (@see DemoAssetClient) intercepts it and serves the embedded bytes.
inline constexpr std::string_view scheme = "demo://";

/// Build a demo:// URL for an embedded asset key (e.g. "covers/fox-lantern.png").
[[nodiscard]] std::string asset_url(std::string_view key);

/// One chapter of a demo title. All chapters serve the same embedded page set —
/// the demo carries one drawn chapter and reuses it, which is all a showcase needs.
struct DemoChapter {
	long        number;   ///< Chapter number, shown and used as the opaque id.
	std::string title;    ///< Chapter title.
};

/// One work another title declares itself related to (a cross-media entry).
struct DemoRelation {
	std::string kind;   ///< Relation label ("Адаптация", "Приквел", ...).
	std::string title;  ///< The related work's title.
};

/// A single fictional title in the demo catalogue.
struct DemoTitle {
	int                       id;              ///< Stable numeric id (routing/serialize key).
	std::string               slug;            ///< URL slug.
	std::string               title;           ///< Display title.
	std::string               original_title;  ///< Original-language title; empty when none.
	std::string               description;     ///< Plain-text blurb.
	double                    rating;          ///< 0-10 score; 0 = unrated.
	int                       year;            ///< Release year; 0 = unknown.
	bool                      ongoing;         ///< true = ongoing, false = completed.
	std::string               author;          ///< Author name; empty when none.
	std::string               artist;          ///< Artist name; empty when none.
	std::string               cover_key;       ///< Embedded cover asset key.
	std::vector<std::string>  tags;            ///< Genre/theme tags (each also a search token).
	std::vector<std::string>  characters;      ///< Named characters — the open-vocabulary
	                                           ///< axis completed via suggest(), not enumerated.
	std::vector<DemoChapter>  chapters;        ///< Chapter list.
	std::vector<DemoRelation> related;         ///< Declared related works.
};

/// The whole demo catalogue, browse order.
[[nodiscard]] const std::vector<DemoTitle>& catalog();

/// Look up a title by its numeric id; nullptr when none matches.
[[nodiscard]] const DemoTitle* find_by_id(int id);

/// Look up a title by its slug; nullptr when none matches.
[[nodiscard]] const DemoTitle* find_by_slug(std::string_view slug);

/// The embedded page keys of one drawn chapter, in reading order — what every
/// chapter of every title serves.
[[nodiscard]] const std::vector<std::string>& chapter_page_keys();

} // namespace aniparse::parsers::demo
