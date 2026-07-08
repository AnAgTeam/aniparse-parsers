/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/kitsu/KitsuMangaRootGetter.hpp"
#include "aniparse/parsers/kitsu/KitsuMangaGetter.hpp"
#include "aniparse/parsers/kitsu/detail/KitsuApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/utility/Coroutines.hpp"
#include "aniparse/utility/Format.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <optional>

namespace aniparse::parsers {

namespace {
	/// Kitsu caps page[limit] at 20.
	constexpr pageoff max_page_limit = 20;

	/// Map an aniparse sort order to a Kitsu `sort` value (a `-` prefix means
	/// descending); nullopt if the key is unsupported.
	std::optional<std::string> kitsu_sort(const SortOrder& sort) {
		std::string_view field;
		if (sort.key == sort_keys::popularity)        field = "userCount";
		else if (sort.key == sort_keys::rating)       field = "averageRating";
		else if (sort.key == sort_keys::release_time) field = "startDate";
		else return std::nullopt;
		return sort.ascending ? std::string(field) : "-" + std::string(field);
	}

	/// The manga sorts this getter maps onto Kitsu `sort` fields, both directions.
	SupportedSorts supported_manga_sorts() {
		const SortDescriptor both{ .ascending = true, .descending = true };
		SupportedSorts sorts;
		sorts.emplace(std::string(sort_keys::popularity),   both);
		sorts.emplace(std::string(sort_keys::rating),       both);
		sorts.emplace(std::string(sort_keys::release_time), both);
		return sorts;
	}

	/// Parse a {"data":[manga,...]} collection into a page of manga getters, each
	/// carrying its short-card preview so preview_info needs no extra fetch.
	PageResults<std::unique_ptr<MangaGetter>> build_manga_page(
	    const boost::json::value& envelope, const GetFilters& filters) {
		PageResults<std::unique_ptr<MangaGetter>> results;
		const boost::json::array* items = kitsu::data_array(envelope);
		if (!items) {
			return results;
		}
		for (const boost::json::value& entry : *items) {
			const boost::json::object* resource = entry.if_object();
			if (!resource) {
				continue;
			}
			std::string ref = aniparse::json::str(*resource, "id");
			if (ref.empty()) {
				continue;
			}
			results.results.emplace_back(
			    std::make_unique<KitsuMangaGetter>(std::move(ref), kitsu::media_to_preview(*resource)),
			    filters.from + static_cast<pageoff>(results.results.size()));
		}
		results.total_count = kitsu::meta_count(envelope);
		results.next_offset = filters.from + static_cast<pageoff>(results.results.size());
		return results;
	}

	/// A /manga collection request with offset paging + sort applied.
	GetRequest list_request(const RequestorContext& context, const GetFilters& filters,
	                        std::string_view default_sort) {
		GetRequest request = { .url = format("{}/manga", kitsu::get_api_base(context)) };
		const pageoff limit = std::min<pageoff>(
		    filters.limit == page_no_limit ? max_page_limit : static_cast<pageoff>(filters.limit),
		    max_page_limit);
		request.url_params.add("page[limit]",  std::to_string(limit));
		request.url_params.add("page[offset]", std::to_string(filters.from));
		std::string sort{ default_sort };
		if (filters.sort) {
			if (std::optional<std::string> mapped = kitsu_sort(*filters.sort)) {
				sort = std::move(*mapped);
			}
		}
		request.url_params.add("sort", sort);
		return request;
	}
} // namespace

NetworkRequestTask<SearchCompatibilities> KitsuMangaRootGetter::search_support(RequestorContext) {
	// Sorts are static. Categories are a large taxonomy (thousands of paged
	// entries); enumerating them as filter options is deferred — a real impl
	// would page + cache them (cf. the LibSocial parser's LibSocialCatalog).
	co_return SearchCompatibilities{
	    .supported_sorts = supported_manga_sorts(),
	};
}

MangaGetterRootCompatibilities KitsuMangaRootGetter::latest_support() const noexcept {
	return MangaGetterRootCompatibilities{};
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> KitsuMangaRootGetter::search(
    RequestorContext context, SearchRequestQuery query, GetFilters filters) {
	GetRequest request = list_request(context, filters, "-userCount");
	if (!query.query.empty()) {
		request.url_params.add("filter[text]", query.query);
	}

	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	co_return build_manga_page(*json_result, filters);
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> KitsuMangaRootGetter::latest(
    RequestorContext context, GetFilters filters) {
	GetRequest request = list_request(context, filters, "-startDate");

	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	co_return build_manga_page(*json_result, filters);
}

NetworkRequestTask<std::unique_ptr<MangaGetter>> KitsuMangaRootGetter::parse_url(
    RequestorContext, ParsedUrl url) {
	// The ref (slug or numeric id) is the last path segment; info() resolves it.
	std::optional<std::string> ref = kitsu::extract_ref(url.path());
	if (!ref) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "URL carries no Kitsu manga ref");
	}
	co_return std::make_unique<KitsuMangaGetter>(std::move(*ref));
}

NetworkRequestTask<std::unique_ptr<MangaGetter>> KitsuMangaRootGetter::from_serialized(
    SerializedGetterData data) {
	// Inverse of KitsuMangaGetter::serialize(): the ref rides in url.
	if (data.url.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "serialized Kitsu getter carries no ref");
	}
	co_return std::make_unique<KitsuMangaGetter>(std::move(data.url));
}

} // namespace aniparse::parsers
