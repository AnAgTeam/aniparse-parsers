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
			results.append(filters.from,
			    std::make_unique<KitsuMangaGetter>(std::move(ref), kitsu::media_to_preview(*resource)));
		}
		results.total_count = kitsu::meta_count(envelope);
		return results;
	}

	/// The tokens selected on an identity axis, comma-joined for a Kitsu filter[...]
	/// (its filters take a comma list, so a whole selection resolves in ONE request —
	/// which is what the identity keys promise). Empty when the caller did not use the
	/// key. Nullopt when a token is not a positive integer: every vocabulary Kitsu maps
	/// is numeric, and a token that is not one is a caller bug worth reporting, never a
	/// token to quietly drop (a dropped lookup returns a plausible wrong page).
	std::optional<std::string> selected_ids(const SearchItems& filters, std::string_view key) {
		auto entry = filters.find(key);
		if (entry == filters.end()) {
			return std::string{};
		}
		const ItemSelection* selection = std::get_if<ItemSelection>(&entry->second);
		if (!selection) {
			return std::string{};
		}
		std::string joined;
		for (const auto& [token, item] : *selection) {
			const bool numeric = !token.empty() && std::all_of(token.begin(), token.end(),
			    [](unsigned char c) { return std::isdigit(c) != 0; });
			if (!numeric) {
				return std::nullopt;
			}
			if (!joined.empty()) {
				joined += ',';
			}
			joined += token;
		}
		return joined;
	}

	/// Kitsu's spelling of a foreign vocabulary in /mappings (its externalSite), for
	/// the identity keys this source accepts. Empty for a key that is not one.
	std::string_view external_site(std::string_view key) {
		if (key == search_keys::mal_id)     return "myanimelist/manga";
		if (key == search_keys::anilist_id) return "anilist/manga";
		return {};
	}

	/// Parse a /mappings?include=item envelope into a page of getters. The mappings in
	/// "data" carry no metadata — the manga they point at ride in "included".
	PageResults<std::unique_ptr<MangaGetter>> build_mapped_page(
	    const boost::json::value& envelope, const GetFilters& filters) {
		PageResults<std::unique_ptr<MangaGetter>> results;
		const boost::json::array* included =
		    aniparse::json::array_field(envelope.if_object(), "included");
		if (!included) {
			return results;
		}
		for (const boost::json::value& entry : *included) {
			const boost::json::object* resource = entry.if_object();
			if (!resource || aniparse::json::str(*resource, "type") != "manga") {
				continue;
			}
			std::string ref = aniparse::json::str(*resource, "id");
			if (ref.empty()) {
				continue;
			}
			results.append(filters.from,
			    std::make_unique<KitsuMangaGetter>(std::move(ref), kitsu::media_to_preview(*resource)));
		}
		results.total_count = results.results.size();
		return results;
	}

	/// A /manga collection request with offset paging + sort applied.
	GetRequest list_request(const RequestorContext& context, const GetFilters& filters,
	                        std::string_view default_sort) {
		GetRequest request = { .url = fmt::format("{}/manga", kitsu::get_api_base(context)) };
		const pageoff limit = clamp_limit(filters, max_page_limit, max_page_limit);
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

NetworkRequestTask<SearchCompatibilities> KitsuMangaRootGetter::search_support(RequestorContext) const {
	// Sorts are static. Categories are a large taxonomy (thousands of paged
	// entries); enumerating them as filter options is deferred — a real impl
	// would page + cache them (a source with an enumerable catalog would cache it).
	SearchItems filters;
	// kitsu_id is the mandatory own-vocabulary lookup: Kitsu ids are what this source
	// hands out in other sources' ExternalIds, and a consumer holding one and no getter
	// has no other way back in. mal_id/anilist_id are the bonus — Kitsu maps both, so a
	// MAL id picked up anywhere else opens the item here (@see /mappings).
	// Open vocabularies: any token is a candidate id, resolved in one request.
	filters.emplace(std::string(search_keys::kitsu_id),   ItemSelection{});
	filters.emplace(std::string(search_keys::mal_id),     ItemSelection{});
	filters.emplace(std::string(search_keys::anilist_id), ItemSelection{});

	co_return SearchCompatibilities{
	    .supported_filters = std::move(filters),
	    .supported_sorts   = supported_manga_sorts(),
	};
}

MangaGetterRootCompatibilities KitsuMangaRootGetter::latest_support() const noexcept {
	return MangaGetterRootCompatibilities{};
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> KitsuMangaRootGetter::search(
    RequestorContext context, SearchRequestQuery query, GetFilters filters) const {
	// Reject an unsupported filter/sort up front with a typed error, rather than
	// letting the API silently drop it. Kitsu's search_support is cheap (static
	// sorts, no network), so this costs nothing extra.
	auto support = co_await search_support(context);
	if (!support) {
		co_return unexpected(std::move(support.error()));
	}
	if (auto errors = validate_query(*support, query, filters); !errors.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              describe_search_query_errors(errors));
	}

	// An identity key is a lookup, not a filter: the token names the item outright, so
	// it decides the request rather than narrowing one. Kitsu's own ids address /manga
	// directly; a foreign vocabulary goes through /mappings, which resolves the whole
	// selection in one call and sideloads the manga it points at.
	for (const std::string_view key : { search_keys::kitsu_id, search_keys::mal_id, search_keys::anilist_id }) {
		std::optional<std::string> ids = selected_ids(query.filters, key);
		if (!ids) {
			co_return make_response_error(RequestErrorCode::InvalidArguments,
			                              fmt::format("{} accepts numeric ids only", key));
		}
		if (ids->empty()) {
			continue;
		}

		const std::string_view site = external_site(key);
		if (site.empty()) { // kitsu_id — our own vocabulary
			GetRequest own = { .url = fmt::format("{}/manga", kitsu::get_api_base(context)) };
			own.url_params.add("filter[id]", *ids);
			own.url_params.add("page[limit]", std::to_string(clamp_limit(filters, max_page_limit, max_page_limit)));
			auto looked_up = co_await context.request_json(own);
			if (!looked_up) {
				co_return unexpected(std::move(looked_up.error()));
			}
			co_return build_manga_page(*looked_up, filters);
		}

		GetRequest mapped = { .url = fmt::format("{}/mappings", kitsu::get_api_base(context)) };
		mapped.url_params.add("filter[externalSite]", std::string(site));
		mapped.url_params.add("filter[externalId]", *ids);
		mapped.url_params.add("include", "item");
		mapped.url_params.add("page[limit]", std::to_string(clamp_limit(filters, max_page_limit, max_page_limit)));
		auto looked_up = co_await context.request_json(mapped);
		if (!looked_up) {
			co_return unexpected(std::move(looked_up.error()));
		}
		co_return build_mapped_page(*looked_up, filters);
	}

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
    RequestorContext context, GetFilters filters) const {
	if (auto errors = validate_latest_filters(filters); !errors.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              describe_search_query_errors(errors));
	}

	GetRequest request = list_request(context, filters, "-startDate");

	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	co_return build_manga_page(*json_result, filters);
}

NetworkRequestTask<std::unique_ptr<MangaGetter>> KitsuMangaRootGetter::parse_url(
    RequestorContext, ParsedUrl url) const {
	// The ref (slug or numeric id) is the last path segment; info() resolves it.
	std::optional<std::string> ref = kitsu::extract_ref(url.path());
	if (!ref) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "URL carries no Kitsu manga ref");
	}
	co_return std::make_unique<KitsuMangaGetter>(std::move(*ref));
}

NetworkRequestTask<std::unique_ptr<MangaGetter>> KitsuMangaRootGetter::from_serialized(
    SerializedGetterData data) const {
	// Inverse of KitsuMangaGetter::serialize(): the ref rides in url.
	if (data.url.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "serialized Kitsu getter carries no ref");
	}
	co_return std::make_unique<KitsuMangaGetter>(std::move(data.url));
}

} // namespace aniparse::parsers
