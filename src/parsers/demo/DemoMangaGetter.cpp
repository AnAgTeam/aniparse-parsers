/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/demo/DemoMangaGetter.hpp"
#include "aniparse/parsers/demo/DemoData.hpp"

#include <chrono>
#include <string>

namespace aniparse::parsers::demo {
namespace {

/// The year the title started, as a year-precision date (Jan 1 of that year).
std::optional<ModelDate> year_date(int year) {
	if (year <= 0) {
		return std::nullopt;
	}
	const auto ymd = std::chrono::year{ year } / std::chrono::January / 1;
	return ModelDate{ .time = std::chrono::sys_days{ ymd }, .precision = DatePrecision::Year };
}

/// The fields a listing card and a detail view share.
void fill_common(MangaInfo& info, const DemoTitle& t) {
	info.id           = t.id;
	info.common.title = t.title;
	if (t.rating > 0) {
		info.common.rating = Rating::from_score(t.rating, 10);
	}
	info.common.release_time = year_date(t.year);
	if (!t.cover_key.empty()) {
		info.common.previews.push_back(Image{ .url = asset_url(t.cover_key) });
	}
	for (const auto& tag : t.tags) {
		info.common.tags.push_back(Tag{ .name = tag, .ref = tag });
	}
}

/// The short-card info a listing hands to a getter — light, no description.
MangaInfo preview_of(const DemoTitle& t) {
	MangaInfo info;
	fill_common(info, t);
	return info;
}

/// The full record info() returns.
MangaInfo full_of(const DemoTitle& t) {
	MangaInfo info;
	fill_common(info, t);
	if (!t.original_title.empty() && t.original_title != t.title) {
		info.common.original_title = t.original_title;
	}
	info.common.description = AttributedText{ .text = t.description };
	info.common.status      = AiredStatus{
	    .name = std::string(t.ongoing ? aired_status_ongoing : aired_status_released),
	};
	if (!t.author.empty()) {
		info.author = RelatedUser{ .name = t.author };
	}
	if (!t.artist.empty()) {
		info.artist = RelatedUser{ .name = t.artist };
	}
	info.total_chapters = static_cast<long>(t.chapters.size());
	return info;
}

} // namespace

// Exposed so the root getter builds the same card without duplicating the shape.
MangaInfo demo_preview(const DemoTitle& t) { return preview_of(t); }

DemoMangaGetter::DemoMangaGetter(int id, std::optional<MangaInfo> preview)
    : id_(id), preview_(std::move(preview)) {}

MangaGetterCompatibilities DemoMangaGetter::compatibilities() const noexcept {
	return {};
}

std::optional<MangaInfo> DemoMangaGetter::preview_info() const noexcept {
	return preview_;
}

NetworkRequestTask<MangaInfo> DemoMangaGetter::info(RequestorContext /*context*/) const {
	const DemoTitle* t = find_by_id(id_);
	if (!t) {
		co_return make_response_error(RequestErrorCode::NotFound, "demo title not found");
	}
	co_return full_of(*t);
}

NetworkRequestTask<PageResults<MangaChapterInfo>> DemoMangaGetter::chapters_info(
    RequestorContext /*context*/, GetFilters filters,
    std::optional<MangaTranslationID> /*translation*/) const {
	PageResults<MangaChapterInfo> out;
	const DemoTitle* t = find_by_id(id_);
	if (!t) {
		co_return make_response_error(RequestErrorCode::NotFound, "demo title not found");
	}
	const auto& chapters = t->chapters;
	for (pageoff i = filters.from; i < static_cast<pageoff>(chapters.size()); ++i) {
		if (out.results.size() >= filters.limit) {
			break;
		}
		const DemoChapter& c = chapters[static_cast<std::size_t>(i)];
		MangaChapterInfo info;
		info.chapter = c.number;
		info.id      = std::to_string(c.number);   // opaque handle, round-trips via ref()
		info.name    = c.title;
		out.append(filters.from, std::move(info));
	}
	out.total_count = chapters.size();
	co_return out;
}

NetworkRequestTask<PageResults<MangaPage>> DemoMangaGetter::chapter_pages(
    RequestorContext /*context*/, MangaChapterRef /*chapter*/, GetFilters filters,
    std::optional<MangaTranslationID> /*translation*/) const {
	// Every chapter serves the one drawn chapter the demo carries.
	PageResults<MangaPage> out;
	const auto& keys = chapter_page_keys();
	for (pageoff i = filters.from; i < static_cast<pageoff>(keys.size()); ++i) {
		if (out.results.size() >= filters.limit) {
			break;
		}
		MangaPage page;
		page.image = Image{ .url = asset_url(keys[static_cast<std::size_t>(i)]) };
		out.append(filters.from, std::move(page));
	}
	out.total_count = keys.size();
	co_return out;
}

NetworkRequestTask<PageResults<RelatedWork>> DemoMangaGetter::related(
    RequestorContext /*context*/, GetFilters filters) const {
	PageResults<RelatedWork> out;
	const DemoTitle* t = find_by_id(id_);
	if (!t || filters.from > 0) {   // the whole (small) list is one page
		co_return out;
	}
	for (const auto& r : t->related) {
		RelatedWork work;
		work.kind     = MediaKind::Manga;
		work.relation = r.kind;
		work.title    = r.title;
		// When the related work is itself in the catalogue, make it openable.
		for (const auto& other : catalog()) {
			if (other.title == r.title) {
				work.handle = SerializedGetterData{ .url = std::to_string(other.id) };
				if (!other.cover_key.empty()) {
					work.previews.push_back(Image{ .url = asset_url(other.cover_key) });
				}
				break;
			}
		}
		out.append(0, std::move(work));
	}
	co_return out;
}

NetworkRequestTask<SerializedGetterData> DemoMangaGetter::serialize() const {
	co_return SerializedGetterData{ .url = std::to_string(id_) };
}

} // namespace aniparse::parsers::demo
