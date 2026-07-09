/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/gelbooru/GelbooruImagesGetter.hpp"
#include "aniparse/parsers/gelbooru/GelbooruContainerGetter.hpp"
#include "aniparse/parsers/gelbooru/detail/GelbooruApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/utility/Coroutines.hpp"
#include "aniparse/utility/Format.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <optional>

namespace aniparse::parsers {

namespace {
	namespace json = aniparse::json;

	/// Gelbooru caps /post index at 100 items per page; a modest default otherwise.
	constexpr pageoff max_page_limit = 100;
	constexpr pageoff default_limit  = 20;

	/// Map an aniparse sort onto a Gelbooru `sort:` metatag; nullopt if unsupported.
	std::optional<std::string> gelbooru_sort(const SortOrder& sort) {
		if (sort.key == sort_keys::popularity) {
			return sort.ascending ? "sort:score:asc" : "sort:score:desc";
		}
		if (sort.key == sort_keys::release_time) {
			return sort.ascending ? "sort:id:asc" : "sort:id:desc";
		}
		return std::nullopt;
	}

	SupportedSorts supported_sorts() {
		const SortDescriptor both{ .ascending = true, .descending = true };
		SupportedSorts sorts;
		sorts.emplace(std::string(sort_keys::popularity),   both);
		sorts.emplace(std::string(sort_keys::release_time), both);
		return sorts;
	}

	/// Map a Gelbooru autocomplete category string to a search-key axis.
	std::optional<std::string> category_axis(std::string_view category) {
		if (category == "artist")    return std::string(search_keys::artist);
		if (category == "character") return std::string(search_keys::character);
		if (category == "copyright") return std::string(search_keys::series);
		return std::nullopt;
	}

	/// A /post index request with tag string + pid/limit paging. The sort, if any,
	/// is folded into the tag string as a `sort:` metatag.
	GetRequest list_request(const RequestorContext& context, std::string tags,
	                        const GetFilters& filters) {
		const pageoff limit = std::min<pageoff>(
		    filters.limit == page_no_limit ? default_limit : static_cast<pageoff>(filters.limit),
		    max_page_limit);
		// Gelbooru pages by 0-based pid: pid = from / limit.
		const pageoff pid = limit > 0 ? filters.from / limit : 0;

		if (filters.sort) {
			if (std::optional<std::string> sort = gelbooru_sort(*filters.sort)) {
				if (!tags.empty()) {
					tags += ' ';
				}
				tags += *sort;
			}
		}

		GetRequest request = { .url = format("{}/index.php", gelbooru::get_api_base(context)) };
		request.url_params.add("page", "dapi");
		request.url_params.add("s",    "post");
		request.url_params.add("q",    "index");
		request.url_params.add("json", "1");
		if (!tags.empty()) {
			request.url_params.add("tags", std::move(tags));
		}
		request.url_params.add("pid",   std::to_string(pid));
		request.url_params.add("limit", std::to_string(limit));
		return request;
	}

	/// Parse a DAPI envelope into a page of container getters, each carrying its
	/// mapped info + media leaf so info()/items() need no refetch.
	PageResults<std::unique_ptr<ImageContainerGetter>> build_post_page(
	    const boost::json::value& envelope, const GetFilters& filters) {
		PageResults<std::unique_ptr<ImageContainerGetter>> results;
		const boost::json::array* posts = gelbooru::posts_of(envelope);
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
			    std::make_unique<GelbooruContainerGetter>(
			        id,
			        gelbooru::post_to_container_info(*post),
			        gelbooru::post_to_item(*post)),
			    filters.from + static_cast<pageoff>(results.results.size()));
		}
		results.next_offset = filters.from + static_cast<pageoff>(results.results.size());
		return results;
	}
} // namespace

NetworkRequestTask<SearchCompatibilities> GelbooruImagesGetter::search_support(RequestorContext) {
	// Tags are open-vocabulary free text; only the mappable sorts and autocomplete
	// are declared. Autocomplete has a single default kind (all tags), so no kinds
	// are advertised.
	co_return SearchCompatibilities{
	    .supported_sorts = supported_sorts(),
	    .compatibilities = compatibilities_flags::default_flags | compatibilities_flags::supports_suggestions,
	};
}

NetworkRequestTask<std::vector<SearchSuggestion>> GelbooruImagesGetter::suggest(
    RequestorContext context, std::string partial, std::optional<std::string>) {
	// autocomplete2 is the one credential-free surface; a single tag_query type.
	GetRequest request = { .url = format("{}/index.php", gelbooru::get_api_base(context)) };
	request.url_params.add("page", "autocomplete2");
	request.url_params.add("term", std::move(partial));
	request.url_params.add("type", "tag_query");
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
		suggestion.value = json::str(item, "value");
		if (suggestion.value.empty()) {
			continue;
		}
		suggestion.label = json::str(item, "label");
		if (suggestion.label.empty()) {
			suggestion.label = suggestion.value;
		}
		// Gelbooru reports post_count as a STRING; parse it leniently.
		if (std::string count = json::str(item, "post_count"); !count.empty()) {
			if (long parsed = std::atol(count.c_str()); parsed > 0) {
				suggestion.count = parsed;
			}
		}
		suggestion.category = category_axis(json::str(item, "category"));
		suggestions.push_back(std::move(suggestion));
	}
	co_return suggestions;
}

NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> GelbooruImagesGetter::search(
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

NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> GelbooruImagesGetter::latest(
    RequestorContext context, GetFilters filters) {
	if (auto errors = validate_latest_filters(filters); !errors.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              describe_search_query_errors(errors));
	}

	GetRequest request = list_request(context, std::string{}, filters);
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	co_return build_post_page(*json_result, filters);
}

NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> GelbooruImagesGetter::parse_url(
    RequestorContext, ParsedUrl url) {
	std::optional<ImageContainerID> id = gelbooru::extract_id(url.query());
	if (!id) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "URL carries no Gelbooru post id");
	}
	co_return std::make_unique<GelbooruContainerGetter>(*id);
}

NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> GelbooruImagesGetter::from_serialized(
    SerializedGetterData data) {
	if (data.url.empty()) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		                              "serialized Gelbooru getter carries no id");
	}
	auto id = static_cast<ImageContainerID>(std::atol(data.url.c_str()));
	co_return std::make_unique<GelbooruContainerGetter>(id);
}

} // namespace aniparse::parsers
