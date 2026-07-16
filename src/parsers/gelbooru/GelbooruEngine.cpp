/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/gelbooru/GelbooruEngine.hpp"
#include "aniparse/parsers/gelbooru/detail/GelbooruApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/utility/Format.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <cstdlib>
#include <optional>
#include <string>

namespace aniparse::parsers::gelbooru {

namespace {
	namespace json = aniparse::json;

	/// Gelbooru caps the /post index at 100 items per page; a modest default otherwise.
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

	/// Stamp the DAPI post-index selector params shared by search and single-post.
	void add_post_index_params(GetRequest& request) {
		request.url_params.add("page", "dapi");
		request.url_params.add("s",    "post");
		request.url_params.add("q",    "index");
		request.url_params.add("json", "1");
	}

	class GelbooruEngine final : public engines::BooruEngine {
	public:
		Headers api_headers() const override { return gelbooru::api_headers(); }

		SearchCompatibilities search_support() const override {
			// Tags are open-vocabulary free text; only the mappable sorts and
			// autocomplete are declared. Autocomplete has a single default kind (all
			// tags), so no kinds are advertised.
			return SearchCompatibilities{
			    .supported_sorts = supported_sorts(),
			    .compatibilities = compatibilities_flags::default_flags
			                     | compatibilities_flags::supports_suggestions,
			};
		}

		GetRequest list_request(std::string_view base, std::string tags,
		                        const GetFilters& filters) const override {
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

			GetRequest request = { .url = fmt::format("{}/index.php", base) };
			add_post_index_params(request);
			if (!tags.empty()) {
				request.url_params.add("tags", std::move(tags));
			}
			request.url_params.add("pid",   std::to_string(pid));
			request.url_params.add("limit", std::to_string(limit));
			return request;
		}

		const boost::json::array* posts_of(const boost::json::value& envelope) const override {
			return gelbooru::posts_of(envelope);
		}

		GetRequest container_request(std::string_view base, ImageContainerID id) const override {
			GetRequest request = { .url = fmt::format("{}/index.php", base) };
			add_post_index_params(request);
			request.url_params.add("id", std::to_string(id));
			return request;
		}

		const boost::json::object* single_post(const boost::json::value& envelope) const override {
			const boost::json::array* posts = gelbooru::posts_of(envelope);
			if (!posts || posts->empty()) {
				return nullptr;
			}
			return posts->front().if_object();
		}

		ImageContainerInfo post_to_container_info(
		    const boost::json::object& post,
		    std::optional<std::string_view> media_referer) const override {
			return gelbooru::post_to_container_info(post, media_referer);
		}

		std::optional<ImageItem> post_to_item(
		    const boost::json::object& post,
		    std::optional<std::string_view> media_referer) const override {
			return gelbooru::post_to_item(post, media_referer);
		}

		GetRequest suggest_request(std::string_view base, std::string partial,
		                           std::optional<std::string> /*kind*/) const override {
			// autocomplete2 is the one credential-free surface; a single tag_query type.
			GetRequest request = { .url = fmt::format("{}/index.php", base) };
			request.url_params.add("page", "autocomplete2");
			request.url_params.add("term", std::move(partial));
			request.url_params.add("type", "tag_query");
			request.url_params.add("limit", "10");
			return request;
		}

		std::vector<SearchSuggestion> parse_suggestions(
		    const boost::json::value& envelope) const override {
			std::vector<SearchSuggestion> suggestions;
			const boost::json::array* items = envelope.if_array();
			if (!items) {
				return suggestions;
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
			return suggestions;
		}

		std::optional<ImageContainerID> post_id_from_url(const ParsedUrl& url) const override {
			// Gelbooru addresses by query param, not path.
			return gelbooru::extract_id(url.query());
		}
	};
} // namespace

const engines::BooruEngine& engine() {
	static const GelbooruEngine instance;
	return instance;
}

} // namespace aniparse::parsers::gelbooru
