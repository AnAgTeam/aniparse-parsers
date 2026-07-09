/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/danbooru/DanbooruPoolGetter.hpp"
#include "aniparse/parsers/danbooru/detail/DanbooruApi.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/Coroutines.hpp"
#include "aniparse/utility/Format.hpp"

#include <boost/json.hpp>

#include <algorithm>

namespace aniparse::parsers {

namespace {
	/// Danbooru caps /posts.json at 200 items per page; a modest default otherwise.
	constexpr pageoff max_page_limit = 200;
	constexpr pageoff default_limit  = 20;
} // namespace

DanbooruPoolGetter::DanbooruPoolGetter(ImageContainerID id) : id_(id) {}

ImageContainerCompatibilities DanbooruPoolGetter::compatibilities() const noexcept {
	return {};
}

NetworkRequestTask<std::monostate> DanbooruPoolGetter::ensure_info(RequestorContext& context) {
	if (info_) {
		co_return std::monostate{};
	}

	GetRequest request = { .url = format("{}/pools/{}.json", danbooru::get_api_base(context), id_) };
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}

	const boost::json::object* pool = json_result->if_object();
	if (!pool) {
		co_return make_response_error(RequestErrorCode::NotFound, "Danbooru pool not found");
	}

	info_ = danbooru::pool_to_container_info(*pool);
	co_return std::monostate{};
}

NetworkRequestTask<ImageContainerInfo> DanbooruPoolGetter::info(RequestorContext context) {
	if (auto loaded = co_await ensure_info(context); !loaded) {
		co_return unexpected(std::move(loaded.error()));
	}
	co_return *info_;
}

NetworkRequestTask<PageResults<ImageItem>> DanbooruPoolGetter::items(
    RequestorContext context, GetFilters filters) {
	const pageoff limit = std::min<pageoff>(
	    filters.limit == page_no_limit ? default_limit : static_cast<pageoff>(filters.limit),
	    max_page_limit);
	// GetFilters::from is a 0-based item offset; Danbooru pages by 1-based page number.
	const pageoff page = limit > 0 ? filters.from / limit + 1 : 1;

	// `ordpool:{id}` returns the pool's posts in pool order (paged like any listing),
	// so items() streams the collection without holding the whole id list.
	GetRequest request = { .url = format("{}/posts.json", danbooru::get_api_base(context)) };
	request.url_params.add("tags", format("ordpool:{}", id_));
	request.url_params.add("page",  std::to_string(page));
	request.url_params.add("limit", std::to_string(limit));

	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}

	PageResults<ImageItem> results;
	const boost::json::array* posts = json_result->if_array();
	if (!posts) {
		co_return results;
	}
	for (const boost::json::value& entry : *posts) {
		const boost::json::object* post = entry.if_object();
		if (!post) {
			continue;
		}
		// A banned/deleted post in the pool has no servable file — skip it rather
		// than yield a media-less item.
		if (std::optional<ImageItem> item = danbooru::post_to_item(*post)) {
			results.results.push_back(PageItem<ImageItem>{
			    .item = std::move(*item),
			    .offset = filters.from + static_cast<pageoff>(results.results.size()),
			});
		}
	}
	results.next_offset = filters.from + static_cast<pageoff>(results.results.size());
	co_return results;
}

NetworkRequestTask<SerializedGetterData> DanbooruPoolGetter::serialize() {
	// "pools/" prefix distinguishes a pool from a bare post id on restore.
	co_return SerializedGetterData{ .url = format("pools/{}", id_) };
}

} // namespace aniparse::parsers
