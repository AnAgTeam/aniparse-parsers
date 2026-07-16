/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/danbooru/DanbooruEngine.hpp"
#include "aniparse/parsers/danbooru/detail/DanbooruApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/utility/Format.hpp"

#include <boost/json.hpp>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>

namespace aniparse::parsers::danbooru {

namespace {
	/// Danbooru caps /posts.json at 200 items per page; a modest default otherwise.
	constexpr pageoff max_page_limit = 200;
	constexpr pageoff default_limit  = 20;

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

	/// Map a Danbooru tag category id to a search-key axis, so a suggestion is
	/// grouped by what kind of tag it is. General (0) and meta (5) -> untyped.
	std::optional<std::string> category_axis(std::int64_t category) {
		switch (category) {
		case 1: return std::string(search_keys::artist);
		case 3: return std::string(search_keys::series); // Danbooru "copyright" == franchise
		case 4: return std::string(search_keys::character);
		default: return std::nullopt;
		}
	}

	/// The sorts this engine maps onto Danbooru `order:` metatags, both directions.
	SupportedSorts supported_sorts() {
		const SortDescriptor both{ .ascending = true, .descending = true };
		SupportedSorts sorts;
		sorts.emplace(std::string(sort_keys::popularity),   both);
		sorts.emplace(std::string(sort_keys::release_time), both);
		return sorts;
	}

	class DanbooruEngine final : public engines::BooruEngine {
	public:
		Headers api_headers() const override { return danbooru::api_headers(); }

		bool supports_pools() const noexcept override { return true; }

		SearchCompatibilities search_support() const override {
			// Tags are open-vocabulary free text, so no enumerable filter set is
			// advertised; only the mappable sorts. Autocomplete is offered and can
			// narrow to the artist axis (Danbooru's dedicated search[type]=artist).
			return SearchCompatibilities{
			    .supported_sorts = supported_sorts(),
			    .compatibilities = compatibilities_flags::default_flags
			                     | compatibilities_flags::supports_suggestions,
			    .supported_suggestion_kinds = { std::string(search_keys::artist) },
			};
		}

		GetRequest list_request(std::string_view base, std::string tags,
		                        const GetFilters& filters) const override {
			const pageoff limit = clamp_limit(filters, max_page_limit, default_limit);
			// GetFilters::from is a 0-based item offset; Danbooru pages by 1-based page
			// number (deep paging past page 1000 is API-capped).
			const OffsetPaging paging{ .from = filters.from, .want = limit, .stride = limit };

			if (filters.sort) {
				if (std::optional<std::string> order = danbooru_order(*filters.sort)) {
					if (!tags.empty()) {
						tags += ' ';
					}
					tags += *order;
				}
			}

			GetRequest request = { .url = fmt::format("{}/posts.json", base) };
			if (!tags.empty()) {
				request.url_params.add("tags", std::move(tags));
			}
			request.url_params.add("page",  std::to_string(paging.page()));
			request.url_params.add("limit", std::to_string(limit));
			return request;
		}

		const boost::json::array* posts_of(const boost::json::value& envelope) const override {
			// A listing is a bare [post, ...] array.
			return envelope.if_array();
		}

		GetRequest container_request(std::string_view base, ImageContainerID id) const override {
			return GetRequest{ .url = fmt::format("{}/posts/{}.json", base, id) };
		}

		const boost::json::object* single_post(const boost::json::value& envelope) const override {
			// /posts/{id}.json returns an object, not an array.
			return envelope.if_object();
		}

		ImageContainerInfo post_to_container_info(
		    const boost::json::object& post,
		    std::optional<std::string_view> /*media_referer*/) const override {
			// Danbooru's CDN serves media to a bare GET — no Referer to attach.
			return danbooru::post_to_container_info(post);
		}

		std::optional<ImageItem> post_to_item(
		    const boost::json::object& post,
		    std::optional<std::string_view> /*media_referer*/) const override {
			return danbooru::post_to_item(post);
		}

		GetRequest suggest_request(std::string_view base, std::string partial,
		                           std::optional<std::string> kind) const override {
			// The only narrowing Danbooru's autocomplete offers as a distinct type is
			// artist; anything else queries all tags and comes back typed per item.
			std::string type = (kind && *kind == search_keys::artist) ? "artist" : "tag_query";

			GetRequest request = { .url = fmt::format("{}/autocomplete.json", base) };
			request.url_params.add("search[query]", std::move(partial));
			request.url_params.add("search[type]", std::move(type));
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
			return suggestions;
		}

		std::optional<ImageContainerID> post_id_from_url(const ParsedUrl& url) const override {
			return danbooru::extract_post_id(url.path());
		}

		std::optional<ImageContainerID> pool_id_from_url(const ParsedUrl& url) const override {
			return danbooru::extract_pool_id(url.path());
		}

		GetRequest pool_request(std::string_view base, ImageContainerID id) const override {
			return GetRequest{ .url = fmt::format("{}/pools/{}.json", base, id) };
		}

		ImageContainerInfo pool_to_container_info(const boost::json::object& pool) const override {
			return danbooru::pool_to_container_info(pool);
		}

		GetRequest pool_items_request(std::string_view base, ImageContainerID id,
		                              const GetFilters& filters) const override {
			const pageoff limit = std::min<pageoff>(
			    filters.limit == page_no_limit ? default_limit : static_cast<pageoff>(filters.limit),
			    max_page_limit);
			// GetFilters::from is a 0-based item offset; Danbooru pages by 1-based page.
			const pageoff page = limit > 0 ? filters.from / limit + 1 : 1;

			// `ordpool:{id}` returns the pool's posts in pool order, paged like any listing.
			GetRequest request = { .url = fmt::format("{}/posts.json", base) };
			request.url_params.add("tags", fmt::format("ordpool:{}", id));
			request.url_params.add("page",  std::to_string(page));
			request.url_params.add("limit", std::to_string(limit));
			return request;
		}
	};
} // namespace

const engines::BooruEngine& engine() {
	static const DanbooruEngine instance;
	return instance;
}

} // namespace aniparse::parsers::danbooru
