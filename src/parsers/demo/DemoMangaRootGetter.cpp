/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/demo/DemoMangaRootGetter.hpp"
#include "aniparse/parsers/demo/DemoMangaGetter.hpp"
#include "aniparse/parsers/demo/DemoData.hpp"

#include <algorithm>
#include <charconv>
#include <string>
#include <variant>
#include <vector>

namespace aniparse::parsers::demo {
namespace {

std::unique_ptr<MangaGetter> getter_for(const DemoTitle& t) {
	return std::make_unique<DemoMangaGetter>(t.id, demo_preview(t));
}

/// Every distinct tag across the catalogue, in first-seen order — the demo's tag
/// vocabulary, used both for the tag filter's options and for suggestions.
std::vector<std::string> all_tags() {
	std::vector<std::string> tags;
	for (const auto& t : catalog()) {
		for (const auto& tag : t.tags) {
			if (std::find(tags.begin(), tags.end(), tag) == tags.end()) {
				tags.push_back(tag);
			}
		}
	}
	return tags;
}

bool has_tag(const DemoTitle& t, const std::string& tag) {
	return std::find(t.tags.begin(), t.tags.end(), tag) != t.tags.end();
}

bool has_character(const DemoTitle& t, const std::string& name) {
	return std::find(t.characters.begin(), t.characters.end(), name) != t.characters.end();
}

/// Every distinct character across the catalogue — the corpus suggest() completes
/// for the open-vocabulary character axis (which is NOT enumerated in the support
/// table, so it is discovered only through suggestions).
std::vector<std::string> all_characters() {
	std::vector<std::string> names;
	for (const auto& t : catalog()) {
		for (const auto& name : t.characters) {
			if (std::find(names.begin(), names.end(), name) == names.end()) {
				names.push_back(name);
			}
		}
	}
	return names;
}

/// Order a working set by the requested sort key (a proxy where the demo has no
/// real metric for it). No sort requested = catalogue order.
void sort_titles(std::vector<const DemoTitle*>& items, const std::optional<SortOrder>& sort) {
	if (!sort) {
		return;
	}
	const bool asc = sort->ascending;
	auto by = [&](auto proj) {
		std::stable_sort(items.begin(), items.end(),
		    [&](const DemoTitle* a, const DemoTitle* b) {
			    return asc ? proj(*a) < proj(*b) : proj(*b) < proj(*a);
		    });
	};
	if (sort->key == sort_keys::rating || sort->key == sort_keys::popularity) {
		by([](const DemoTitle& t) { return t.rating; });
	} else if (sort->key == sort_keys::release_time) {
		by([](const DemoTitle& t) { return t.year; });
	} else if (sort->key == sort_keys::title) {
		by([](const DemoTitle& t) { return t.title; });
	}
}

/// Sort + page a working set into ready getters. Pagination is uniform with
/// latest/search: honour filters.from / filters.limit and advance next_offset.
PageResults<std::unique_ptr<MangaGetter>> page_of(
    std::vector<const DemoTitle*> items, const GetFilters& filters) {
	sort_titles(items, filters.sort);
	PageResults<std::unique_ptr<MangaGetter>> out;
	for (pageoff i = filters.from; i < static_cast<pageoff>(items.size()); ++i) {
		if (out.results.size() >= filters.limit) {
			break;
		}
		out.append(filters.from, getter_for(*items[static_cast<std::size_t>(i)]));
	}
	out.total_count = items.size();
	return out;
}

/// Case-sensitive substring — enough for the demo's Cyrillic catalogue.
bool contains(std::string_view haystack, std::string_view needle) {
	return needle.empty() || haystack.find(needle) != std::string_view::npos;
}

} // namespace

NetworkRequestTask<SearchCompatibilities> DemoMangaRootGetter::search_support(
    RequestorContext /*context*/) const {
	const SortDescriptor both{ .ascending = true, .descending = true };
	SupportedSorts sorts;
	sorts.emplace(std::string(sort_keys::popularity), both);
	sorts.emplace(std::string(sort_keys::rating), both);
	sorts.emplace(std::string(sort_keys::release_time), both);
	sorts.emplace(std::string(sort_keys::title), both);

	SearchItems filters;
	// Title free-text.
	filters.emplace(std::string(search_keys::title), TextQuery{});
	// Tags as an ENUMERATED selection, each option EXCLUDABLE (exclusive = true): a
	// tag can be required or forbidden. Each key equals a Tag::ref, so a tag tapped
	// on a card round-trips straight back into a query.
	ItemSelection tags;
	for (const auto& tag : all_tags()) {
		tags.emplace(tag, ItemSelectionValue{ .name = tag, .exclusive = true });
	}
	filters.emplace(std::string(search_keys::tag), std::move(tags));
	// Publication status — an enumerated selection, no exclusion.
	ItemSelection status;
	status.emplace(std::string(aired_status_ongoing),  ItemSelectionValue{ .name = "Онгоинг",  .exclusive = false });
	status.emplace(std::string(aired_status_released), ItemSelectionValue{ .name = "Завершён", .exclusive = false });
	filters.emplace(std::string(search_keys::status), std::move(status));
	// An OPEN-VOCABULARY selection: declared empty, so its tokens are not enumerated
	// here — a consumer discovers them through suggest() with this kind, and the
	// chosen token round-trips back into the query. @see suggest
	filters.emplace(std::string(search_keys::character), ItemSelection{});
	// Two ranges, one CLOSED and one OPEN: chapter count is bounded [1, 30] (the
	// interval declares its own limits), while year is unbounded (an empty interval —
	// any value accepted).
	filters.emplace(std::string(search_keys::pages),
	                IntInterval{ .from = 1, .to = 30 });
	filters.emplace(std::string(search_keys::year), IntInterval{});

	co_return SearchCompatibilities{
	    .supported_filters          = std::move(filters),
	    .supported_sorts            = std::move(sorts),
	    .compatibilities            = compatibilities_flags::supports_suggestions
	                                | compatibilities_flags::supports_pagination_uniqueness,
	    .supported_suggestion_kinds = { std::string(search_keys::title),
	                                    std::string(search_keys::tag),
	                                    std::string(search_keys::character) },
	};
}

MangaGetterRootCompatibilities DemoMangaRootGetter::latest_support() const noexcept {
	MangaGetterRootCompatibilities out;
	const SortDescriptor both{ .ascending = true, .descending = true };
	out.supported_sorts.emplace(std::string(sort_keys::popularity), both);
	out.supported_sorts.emplace(std::string(sort_keys::rating), both);
	out.supported_sorts.emplace(std::string(sort_keys::release_time), both);
	out.supported_sorts.emplace(std::string(sort_keys::title), both);
	out.compatibilities = compatibilities_flags::supports_pagination_uniqueness;
	return out;
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> DemoMangaRootGetter::latest(
    RequestorContext /*context*/, GetFilters filters) const {
	std::vector<const DemoTitle*> items;
	for (const auto& t : catalog()) {
		items.push_back(&t);
	}
	co_return page_of(std::move(items), filters);
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> DemoMangaRootGetter::search(
    RequestorContext /*context*/, SearchRequestQuery query, GetFilters filters) const {
	// Pull the structured filters we understand out of the query.
	std::string title_needle = query.query;
	if (auto it = query.filters.find(std::string(search_keys::title)); it != query.filters.end()) {
		if (const auto* tq = std::get_if<TextQuery>(&it->second); tq && !tq->text.empty()) {
			title_needle = tq->text;
		}
	}
	auto selection = [&](std::string_view key) -> const ItemSelection* {
		auto it = query.filters.find(std::string(key));
		return it != query.filters.end() ? std::get_if<ItemSelection>(&it->second) : nullptr;
	};
	auto interval = [&](std::string_view key) -> const IntInterval* {
		auto it = query.filters.find(std::string(key));
		return it != query.filters.end() ? std::get_if<IntInterval>(&it->second) : nullptr;
	};
	const ItemSelection* tag_sel       = selection(search_keys::tag);
	const ItemSelection* status_sel    = selection(search_keys::status);
	const ItemSelection* character_sel = selection(search_keys::character);
	const IntInterval*   year          = interval(search_keys::year);
	const IntInterval*   pages         = interval(search_keys::pages);

	auto in_range = [](const IntInterval* iv, long value) {
		return !iv || ((!iv->from || value >= *iv->from) && (!iv->to || value <= *iv->to));
	};

	std::vector<const DemoTitle*> items;
	for (const auto& t : catalog()) {
		if (!contains(t.title, title_needle)) {
			continue;
		}
		// Tags: options come in two flavours — required (exclusive == false) and
		// forbidden (exclusive == true). A title must carry at least one required tag
		// (when any are named) and none of the forbidden ones.
		if (tag_sel && !tag_sel->empty()) {
			bool has_required = false, wants_required = false, hits_forbidden = false;
			for (const auto& [token, opt] : *tag_sel) {
				if (opt.exclusive) {
					hits_forbidden |= has_tag(t, token);
				} else {
					wants_required = true;
					has_required |= has_tag(t, token);
				}
			}
			if (hits_forbidden || (wants_required && !has_required)) {
				continue;
			}
		}
		// Characters: an open-vocabulary include — any selected character matches.
		if (character_sel && !character_sel->empty()) {
			bool any = false;
			for (const auto& [token, opt] : *character_sel) {
				(void)opt;
				if (has_character(t, token)) { any = true; break; }
			}
			if (!any) {
				continue;
			}
		}
		if (status_sel && !status_sel->empty()) {
			const std::string want(t.ongoing ? aired_status_ongoing : aired_status_released);
			if (!status_sel->contains(want)) {
				continue;
			}
		}
		if (!in_range(year, t.year) ||
		    !in_range(pages, static_cast<long>(t.chapters.size()))) {
			continue;
		}
		items.push_back(&t);
	}
	co_return page_of(std::move(items), filters);
}

NetworkRequestTask<std::vector<SearchSuggestion>> DemoMangaRootGetter::suggest(
    RequestorContext /*context*/, std::string partial,
    std::optional<std::string> kind) const {
	const bool want_titles     = !kind || *kind == search_keys::title;
	const bool want_tags       = !kind || *kind == search_keys::tag;
	const bool want_characters = !kind || *kind == search_keys::character;
	std::vector<SearchSuggestion> out;

	if (want_titles) {
		for (const auto& t : catalog()) {
			if (contains(t.title, partial)) {
				out.push_back(SearchSuggestion{
				    .value    = t.title,
				    .label    = t.title,
				    .category = std::string(search_keys::title),
				});
			}
		}
	}
	if (want_tags) {
		for (const auto& tag : all_tags()) {
			if (contains(tag, partial)) {
				out.push_back(SearchSuggestion{
				    .value    = tag,
				    .label    = tag,
				    .category = std::string(search_keys::tag),
				});
			}
		}
	}
	// The open-vocabulary axis: its options exist only here, not in search_support.
	if (want_characters) {
		for (const auto& name : all_characters()) {
			if (contains(name, partial)) {
				out.push_back(SearchSuggestion{
				    .value    = name,
				    .label    = name,
				    .category = std::string(search_keys::character),
				});
			}
		}
	}
	co_return out;
}

NetworkRequestTask<std::unique_ptr<MangaGetter>> DemoMangaRootGetter::from_serialized(
    SerializedGetterData data) const {
	int id = 0;
	const auto* first = data.url.data();
	const auto* last  = first + data.url.size();
	auto [ptr, ec] = std::from_chars(first, last, id);
	if (ec != std::errc{} || ptr != last || !find_by_id(id)) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "not a demo identity: " + data.url);
	}
	co_return std::make_unique<DemoMangaGetter>(id);
}

} // namespace aniparse::parsers::demo
