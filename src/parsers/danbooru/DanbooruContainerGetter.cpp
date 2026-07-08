/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/danbooru/DanbooruContainerGetter.hpp"
#include "aniparse/parsers/danbooru/detail/DanbooruApi.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/Coroutines.hpp"
#include "aniparse/utility/Format.hpp"

#include <boost/json.hpp>

namespace aniparse::parsers {

DanbooruContainerGetter::DanbooruContainerGetter(ImageContainerID id,
                                                 std::optional<ImageContainerInfo> info,
                                                 std::optional<ImageItem> item)
    : id_(id), info_(std::move(info)), item_(std::move(item)), loaded_(info_.has_value()) {}

ImageContainerCompatibilities DanbooruContainerGetter::compatibilities() const noexcept {
	return {};
}

NetworkRequestTask<std::monostate> DanbooruContainerGetter::ensure_loaded(RequestorContext& context) {
	if (loaded_) {
		co_return std::monostate{};
	}

	GetRequest request = { .url = format("{}/posts/{}.json", danbooru::get_api_base(context), id_) };
	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}

	const boost::json::object* post = json_result->if_object();
	if (!post) {
		co_return make_response_error(RequestErrorCode::NotFound, "Danbooru post not found");
	}

	info_   = danbooru::post_to_container_info(*post);
	item_   = danbooru::post_to_item(*post);
	loaded_ = true;
	co_return std::monostate{};
}

NetworkRequestTask<ImageContainerInfo> DanbooruContainerGetter::info(RequestorContext context) {
	if (auto loaded = co_await ensure_loaded(context); !loaded) {
		co_return unexpected(std::move(loaded.error()));
	}
	co_return *info_;
}

NetworkRequestTask<PageResults<ImageItem>> DanbooruContainerGetter::items(
    RequestorContext context, GetFilters) {
	if (auto loaded = co_await ensure_loaded(context); !loaded) {
		co_return unexpected(std::move(loaded.error()));
	}

	// A post is a container-of-one: at most a single media leaf, ignoring paging.
	PageResults<ImageItem> page;
	if (item_) {
		page.results.push_back(PageItem<ImageItem>{ .item = *item_, .offset = 0 });
		page.total_count = 1;
		page.next_offset = 1;
	}
	co_return page;
}

NetworkRequestTask<SerializedGetterData> DanbooruContainerGetter::serialize() {
	// Identity is the post id; the cached info/item are a fetch-time convenience,
	// not identity, so a restored getter re-resolves via /posts/{id}.json.
	co_return SerializedGetterData{ .url = std::to_string(id_) };
}

} // namespace aniparse::parsers
