/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/danbooru/DanbooruImagesGetter.hpp"
#include "aniparse/parsers/danbooru/DanbooruContainerGetter.hpp"
#include "aniparse/parsers/danbooru/detail/DanbooruApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/utility/Coroutines.hpp"
#include "aniparse/utility/Format.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <optional>

namespace aniparse::parsers {

namespace {
	/// Danbooru caps /posts.json at 200 items per page.
	constexpr pageoff max_page_limit = 200;
	/// A modest default when the caller sets no limit.
	constexpr pageoff default_limit = 20;

	/// Map an aniparse sort order onto a Danbooru `order:` metatag; nullopt if the
	/// key is unsupported. `order:` is exempt from the anonymous 2-tag limit.
	std::optional<std::string> danbooru_order(const SortOrder& sort) {
		if (sort.key == sort_keys::popularity) {
			return sort.ascending ? "order:score_asc" : "order:score";
		}
		if (sort.key == sort_keys::release_time) {
			return sort.ascending ? "order:id" : "order:id_desc";
		}
		return std::nullopt;
	}

	/// Map a Danbooru tag category id to a search-key axis (@see search_keys), so a
	/// suggestion is colored/grouped by what kind of tag it is. General (0) and meta
	/// (5) have no dedicated axis -> untyped.
	std::optional<std::string> category_axis(std::int64_t category) {
		switch (category) {
		case 1: return std::string(search_keys::artist);
		case 3: return std::string(search_keys::series);    // Danbooru "copyright" == franchise
		case 4: return std::string(search_keys::character);
		default: return std::nullopt;
		}
	}

	/// The sorts this getter maps onto Danbooru `order:` metatags, both directions.
	SupportedSorts supported_sorts() {
		const SortDescriptor both{ .ascending = true, .descending = true };
		SupportedSorts sorts;
		sorts.emplace(std::string(sort_keys::popularity),   both);
		sorts.emplace(std::string(sort_keys::release_time), both);
		return sorts;
	}

	/// A /posts.json request with tag string + page/limit paging applied. The sort,
	/// if any, is folded into the tag string as an `order:` metatag.
	GetRequest list_request(const RequestorContext& context, std::string tags,
	                        const GetFilters& filters) {
		const pageoff limit = std::min<pageoff>(
		    filters.limit == page_no_limit ? default_limit : static_cast<pageoff>(filters.limit),
		    max_page_limit);
		// GetFilters::from is a 0-based item offset; Danbooru pages by 1-based page
		// number, so map offset -> page (deep paging past page 1000 is API-capped).
		const pageoff page = limit > 0 ? filters.from / limit + 1 : 1;

		if (filters.sort) {
			if (std::optional<std::string> order = danbooru_order(*filters.sort)) {
				if (!tags.empty()) {
					tags += ' ';
				}
				tags += *order;
			}
		}

		GetRequest request = { .url = format("{}/posts.json", danbooru::get_api_base(context)) };
		if (!tags.empty()) {
			request.url_params.add("tags", std::move(tags));
		}
		request.url_params.add("page",  std::to_string(page));
		request.url_params.add("limit", std::to_string(limit));
		return request;
	}

	/// Parse a bare [post, ...] array into a page of container getters, each
	/// carrying its mapped info + media leaf so info()/items() need no refetch.
	PageResults<std::unique_ptr<ImageContainerGetter>> build_post_page(
	    const boost::json::value& envelope, const GetFilters& filters) {
		PageResults<std::unique_ptr<ImageContainerGetter>> results;
		const boost::json::array* posts = envelope.if_array();
		if (!posts) {
			return results;
		}
		for (const boost::json::value& entry : *posts) {
			const boost::json::object* post = entry.if_object();
			if (!post) {
				continue;
			}
			auto id = static_cast<ImageContainerID>(aniparse::json::integer(*post, "id"));
			if (id == invalid_image_container_id) {
				continue;
			}
			results.results.emplace_back(
			    std::make_unique<DanbooruContainerGetter>(
			        id,
			        danbooru::post_to_container_info(*post),
			        danbooru::post_to_item(*post)),
			    filters.from + static_cast<pageoff>(results.results.size()));
		}
		// The collection endpoint returns no total; report what this page yielded.
		results.next_offset = filters.from + static_cast<pageoff>(results.results.size());
		return results;
	}
} // namespace

NetworkRequestTask<SearchCompatibilities> DanbooruImagesGetter::search_support(RequestorContext) {
	// Tags are open-vocabulary free text (the query string), so no enumerable
	// filter set is advertised; only the mappable sorts are declared. Autocomplete
	// is offered, and it can narrow to the artist axis (Danbooru's dedicated
	// `search[type]=artist`); other categories ride the default tag query.
	co_return SearchCompatibilities{
	    .supported_sorts = supported_sorts(),
	    .compatibilities = compatibilities_flags::default_flags | compatibilities_flags::supports_suggestions,
	    .supported_suggestion_kinds = { std::string(search_keys::artist) },
	};
}

NetworkRequestTask<std::vector<SearchSuggestion>> DanbooruImagesGetter::suggest(
    RequestorContext context, std::string partial, std::optional<std::string> kind) {
	// Danbooru's dedicated autocomplete. The only narrowing it offers as a distinct
	// type is artist; anything else queries all tags and comes back typed per item.
	std::string type = (kind && *kind == search_keys::artist) ? "artist" : "tag_query";

	GetRequest request = { .url = format("{}/autocomplete.json", danbooru::get_api_base(context)) };
	request.url_params.add("search[query]", std::move(partial));
	request.url_params.add("search[type]", std::move(type));
	request.url_params.add("limit", "10");

	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}

	std::vector<SearchSuggestion> suggestions;
	const boost::json::array* items = json_result->if_array();
	if (!items) {
		co_return suggestions;
	}
	for (const boost::json::value& entry : *items) {
		const boost::json::object* item = entry.if_object();
		if (!item) {
			continue;
		}
		SearchSuggestion suggestion;
		suggestion.value = aniparse::json::str(item, "value");
		if (suggestion.value.empty()) {
			continue;
		}
		suggestion.label = aniparse::json::str(item, "label");
		if (suggestion.label.empty()) {
			suggestion.label = suggestion.value;
		}
		if (std::int64_t count = aniparse::json::integer(item, "post_count"); count > 0) {
			suggestion.count = static_cast<long>(count);
		}
		suggestion.category = category_axis(aniparse::json::integer(item, "category"));
		suggestions.push_back(std::move(suggestion));
	}
	co_return suggestions;
}

NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> DanbooruImagesGetter::search(
    RequestorContext context, SearchRequestQuery query, GetFilters filters) {
	auto support = co_await search_support(context);
	if (!support) {
		co_return unexpected(std::move(support.error()));
	}
	if (auto errors = validate_query(*support, query, filters); !errors.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              describe_search_query_errors(errors));
	}

	GetRequest request = list_request(context, query.query, filters);
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	co_return build_post_page(*json_result, filters);
}

NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> DanbooruImagesGetter::latest(
    RequestorContext context, GetFilters filters) {
	if (auto errors = validate_latest_filters(filters); !errors.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              describe_search_query_errors(errors));
	}

	// No tags = the source's default newest-first feed.
	GetRequest request = list_request(context, std::string{}, filters);
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	co_return build_post_page(*json_result, filters);
}

NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> DanbooruImagesGetter::parse_url(
    RequestorContext, ParsedUrl url) {
	std::optional<ImageContainerID> id = danbooru::extract_post_id(url.path());
	if (!id) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "URL carries no Danbooru post id");
	}
	co_return std::make_unique<DanbooruContainerGetter>(*id);
}

NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> DanbooruImagesGetter::from_serialized(
    SerializedGetterData data) {
	// Inverse of DanbooruContainerGetter::serialize(): the post id rides in url.
	if (data.url.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "serialized Danbooru getter carries no id");
	}
	auto id = static_cast<ImageContainerID>(std::atol(data.url.c_str()));
	co_return std::make_unique<DanbooruContainerGetter>(id);
}

} // namespace aniparse::parsers
