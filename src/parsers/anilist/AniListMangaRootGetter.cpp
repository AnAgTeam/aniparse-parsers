/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/anilist/AniListMangaRootGetter.hpp"
#include "aniparse/parsers/anilist/AniListMangaGetter.hpp"
#include "aniparse/parsers/anilist/detail/AniListApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/utility/Coroutines.hpp"

#include <boost/json.hpp>

#include <charconv>
#include <optional>
#include <variant>

namespace aniparse::parsers {

namespace {
	/// AniList's Page API is page-based; we fetch a fixed window and map the
	/// item-offset GetFilters::from onto a page number + intra-page skip.
	constexpr pageoff per_page = 30;

	/// Filter-group key shared between search_support (fills the options) and
	/// search (reads them back), so the two sides never drift.
	constexpr std::string_view filter_key_genres = "genres";

	/// The Page(media) list query: search + sort + genre filter + lookup by AniList
	/// or MyAnimeList id, short media cards. Both id arguments take a list, so a
	/// whole batch of ids resolves in one request — what a consumer restoring a
	/// stored library needs.
	constexpr std::string_view search_query = R"(query ($search: String, $page: Int, $perPage: Int, $sort: [MediaSort], $genres: [String], $ids: [Int], $malIds: [Int]) {
  Page(page: $page, perPage: $perPage) {
    pageInfo { total }
    media(type: MANGA, search: $search, sort: $sort, genre_in: $genres, id_in: $ids, idMal_in: $malIds) {
      id
      idMal
      title { romaji english native }
      coverImage { large }
    }
  }
})";

	/// The available genres, as plain strings.
	constexpr std::string_view genre_query = "query { GenreCollection }";

	/// Map an aniparse sort order to an AniList MediaSort enum value; nullopt if
	/// the key is unsupported. The unsuffixed enum is ascending, "_DESC" descending.
	std::optional<std::string> media_sort(const SortOrder& sort) {
		std::string_view base;
		if (sort.key == sort_keys::popularity)        base = "POPULARITY";
		else if (sort.key == sort_keys::rating)       base = "SCORE";
		else if (sort.key == sort_keys::title)        base = "TITLE_ROMAJI";
		else if (sort.key == sort_keys::update_time)  base = "UPDATED_AT";
		else if (sort.key == sort_keys::release_time) base = "START_DATE";
		else return std::nullopt;
		std::string value(base);
		if (!sort.ascending) {
			value += "_DESC";
		}
		return value;
	}

	/// The manga sorts this getter maps onto MediaSort, both directions.
	SupportedSorts supported_manga_sorts() {
		const SortDescriptor both{ .ascending = true, .descending = true };
		SupportedSorts sorts;
		sorts.emplace(std::string(sort_keys::popularity),   both);
		sorts.emplace(std::string(sort_keys::rating),       both);
		sorts.emplace(std::string(sort_keys::title),        both);
		sorts.emplace(std::string(sort_keys::update_time),  both);
		sorts.emplace(std::string(sort_keys::release_time), both);
		return sorts;
	}

	/// Collect the selected genre tokens (each token IS the genre string AniList
	/// filters by) from a query's "genres" ItemSelection into a JSON array.
	boost::json::array selected_genres(const SearchItems& filters) {
		boost::json::array genres;
		auto entry = filters.find(filter_key_genres);
		if (entry == filters.end()) {
			return genres;
		}
		const ItemSelection* selection = std::get_if<ItemSelection>(&entry->second);
		if (!selection) {
			return genres;
		}
		for (const auto& [token, item] : *selection) {
			if (!token.empty()) {
				genres.emplace_back(boost::json::string(token));
			}
		}
		return genres;
	}

	/// The ids a query looks the manga up by under an identity key, as the [Int]
	/// list AniList's id_in / idMal_in take. Each token IS the id, so an ExternalId
	/// collected off another source drops straight into the query. Empty when the
	/// caller does not use the key; nullopt when a token is not an id at all — that
	/// is a caller bug, and reporting it beats quietly searching for something else.
	std::optional<boost::json::array> selected_ids(const SearchItems& filters, std::string_view key) {
		boost::json::array ids;
		auto entry = filters.find(key);
		if (entry == filters.end()) {
			return ids;
		}
		const ItemSelection* selection = std::get_if<ItemSelection>(&entry->second);
		if (!selection) {
			return ids;
		}
		for (const auto& [token, item] : *selection) {
			int id          = 0;
			const char* end = token.data() + token.size();
			auto [stop, ec] = std::from_chars(token.data(), end, id);
			if (ec != std::errc{} || stop != end || id <= 0) {
				return std::nullopt;
			}
			ids.emplace_back(id);
		}
		return ids;
	}

	/// Parse a {"Page":{"media":[...],"pageInfo":{...}}} payload into a page of
	/// manga getters, each carrying its short-card preview so preview_info needs
	/// no extra fetch.
	PageResults<std::unique_ptr<MangaGetter>> build_media_page(
	    const boost::json::object* data, const GetFilters& filters) {
		PageResults<std::unique_ptr<MangaGetter>> results;
		const boost::json::object* page  = aniparse::json::object_field(data, "Page");
		const boost::json::array*  media = aniparse::json::array_field(page, "media");
		if (!media) {
			return results;
		}
		// GetFilters::from is an item offset; the fetched page starts on a page
		// boundary, so skip into it and number items from the absolute offset.
		const OffsetPaging paging{ .from = filters.from, .stride = per_page };
		for (pageoff index = paging.skip(); index < static_cast<pageoff>(media->size()); ++index) {
			if (results.results.size() >= filters.limit) {
				break;
			}
			const boost::json::object* item = (*media)[static_cast<std::size_t>(index)].if_object();
			if (!item) {
				continue;
			}
			int id = static_cast<int>(aniparse::json::integer(*item, "id"));
			if (id <= 0) {
				continue;
			}
			results.append(filters.from,
			    std::make_unique<AniListMangaGetter>(id, anilist::media_to_preview(*item)));
		}
		if (const boost::json::object* info = aniparse::json::object_field(page, "pageInfo")) {
			results.total_count = static_cast<std::size_t>(aniparse::json::integer(*info, "total"));
		}
		return results;
	}
} // namespace

NetworkRequestTask<SearchCompatibilities> AniListMangaRootGetter::search_support(RequestorContext context) {
	// Genres come from the live GenreCollection; sorts are static. (A cache like
	// the LibSocial parser's LibSocialCatalog would spare the per-call fetch.)
	SearchItems filters;

	// Identity lookups, declared without a fetch: ids are not an enumerable option
	// set, so both axes carry an open vocabulary (any token is a candidate id).
	// anilist_id is the mandatory own-vocabulary lookup — a consumer holding an
	// AniList id and no getter has no other way in. mal_id is the bonus: AniList is
	// one of the few sources that cross-reference a foreign catalogue.
	filters.emplace(std::string(search_keys::anilist_id), ItemSelection{});
	filters.emplace(std::string(search_keys::mal_id), ItemSelection{});

	PostRequest request = {
	    .url  = std::string(anilist::get_api_base(context)),
	    .body = anilist::graphql_body(genre_query, {}),
	};
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		// Cancellation is not degradation: the caller asked to stop, so stop. Folding
		// it into "no genres available" would report success, let search() fire its
		// own request afterwards, and turn a genre query into a bogus InvalidArguments
		// (the filter would look unsupported).
		if (json_result.error().code == RequestErrorCode::Cancelled) {
			co_return unexpected(std::move(json_result.error()));
		}
		// Any other failure: serve sorts alone; the caller can still search by text/sort.
	}
	else {
		const boost::json::object* data = anilist::graphql_data(*json_result);
		if (const boost::json::array* genres = aniparse::json::array_field(data, "GenreCollection")) {
			ItemSelection selection;
			for (const boost::json::value& genre : *genres) {
				const boost::json::string* name = genre.if_string();
				if (!name || name->empty()) {
					continue;
				}
				std::string label(name->c_str(), name->size());
				// Token equals the genre string: AniList filters genre_in by name,
				// so the option round-trips straight back into search().
				selection.emplace(label, ItemSelectionValue{ .name = label, .exclusive = false });
			}
			if (!selection.empty()) {
				filters.emplace(std::string(filter_key_genres), std::move(selection));
			}
		}
	}

	co_return SearchCompatibilities{
	    .supported_filters = std::move(filters),
	    .supported_sorts   = supported_manga_sorts(),
	};
}

MangaGetterRootCompatibilities AniListMangaRootGetter::latest_support() const noexcept {
	// Latest is recency-ordered (UPDATED_AT); no selectable sort.
	return MangaGetterRootCompatibilities{};
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> AniListMangaRootGetter::search(
    RequestorContext context, SearchRequestQuery query, GetFilters filters) {
	// Reject an unsupported filter/sort up front with a typed error, rather than
	// letting the API silently drop it. (search_support fetches the genre catalog;
	// a real deployment would cache it — see search_support's note.)
	auto support = co_await search_support(context);
	if (!support) {
		co_return unexpected(std::move(support.error()));
	}
	if (auto errors = validate_query(*support, query, filters); !errors.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              describe_search_query_errors(errors));
	}

	boost::json::object variables;
	variables["page"]    = OffsetPaging{ .from = filters.from, .stride = per_page }.page();
	variables["perPage"] = per_page;
	if (!query.query.empty()) {
		variables["search"] = query.query;
	}
	// Default to most-popular when the caller requests no order.
	std::string sort = "POPULARITY_DESC";
	if (filters.sort) {
		if (std::optional<std::string> mapped = media_sort(*filters.sort)) {
			sort = std::move(*mapped);
		}
	}
	variables["sort"] = boost::json::array{ boost::json::string(sort) };
	if (boost::json::array genres = selected_genres(query.filters); !genres.empty()) {
		variables["genres"] = std::move(genres);
	}

	// Identity lookups. validate_query passed them as an open vocabulary, so the
	// tokens are checked here — a token that is not an id is rejected outright,
	// never dropped (a silently ignored lookup returns a plausible wrong page).
	std::optional<boost::json::array> ids = selected_ids(query.filters, search_keys::anilist_id);
	if (!ids) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "anilist_id accepts numeric ids only");
	}
	if (!ids->empty()) {
		variables["ids"] = std::move(*ids);
	}

	std::optional<boost::json::array> mal_ids = selected_ids(query.filters, search_keys::mal_id);
	if (!mal_ids) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "mal_id accepts numeric ids only");
	}
	if (!mal_ids->empty()) {
		variables["malIds"] = std::move(*mal_ids);
	}

	PostRequest request = {
	    .url  = std::string(anilist::get_api_base(context)),
	    .body = anilist::graphql_body(search_query, std::move(variables)),
	};
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	if (std::string error = anilist::graphql_error(*json_result); !error.empty()) {
		co_return make_response_error(RequestErrorCode::Unknown, error);
	}
	co_return build_media_page(anilist::graphql_data(*json_result), filters);
}

NetworkRequestTask<PageResults<std::unique_ptr<MangaGetter>>> AniListMangaRootGetter::latest(
    RequestorContext context, GetFilters filters) {
	if (auto errors = validate_latest_filters(filters); !errors.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              describe_search_query_errors(errors));
	}

	boost::json::object variables;
	variables["page"]    = OffsetPaging{ .from = filters.from, .stride = per_page }.page();
	variables["perPage"] = per_page;
	variables["sort"]    = boost::json::array{ boost::json::string("UPDATED_AT_DESC") };

	PostRequest request = {
	    .url  = std::string(anilist::get_api_base(context)),
	    .body = anilist::graphql_body(search_query, std::move(variables)),
	};
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	if (std::string error = anilist::graphql_error(*json_result); !error.empty()) {
		co_return make_response_error(RequestErrorCode::Unknown, error);
	}
	co_return build_media_page(anilist::graphql_data(*json_result), filters);
}

NetworkRequestTask<std::unique_ptr<MangaGetter>> AniListMangaRootGetter::parse_url(
    RequestorContext, ParsedUrl url) {
	// The media id is the numeric segment of the URL path; the getter needs only
	// that, so no network call is required here.
	std::optional<int> id = anilist::extract_media_id(url.path());
	if (!id) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "URL carries no AniList manga id");
	}
	co_return std::make_unique<AniListMangaGetter>(*id);
}

NetworkRequestTask<std::unique_ptr<MangaGetter>> AniListMangaRootGetter::from_serialized(
    SerializedGetterData data) {
	// Inverse of AniListMangaGetter::serialize(): the media id rides in url.
	int id = 0;
	auto [end, ec] = std::from_chars(data.url.data(), data.url.data() + data.url.size(), id);
	if (ec != std::errc{} || end != data.url.data() + data.url.size() || id <= 0) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "serialized AniList getter carries no valid media id");
	}
	co_return std::make_unique<AniListMangaGetter>(id);
}

} // namespace aniparse::parsers
