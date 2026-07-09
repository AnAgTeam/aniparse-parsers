/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/gelbooru/GelbooruContainerGetter.hpp"
#include "aniparse/parsers/gelbooru/detail/GelbooruApi.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/Coroutines.hpp"
#include "aniparse/utility/Format.hpp"

#include <boost/json.hpp>

namespace aniparse::parsers {

GelbooruContainerGetter::GelbooruContainerGetter(ImageContainerID id,
                                                 std::optional<ImageContainerInfo> info,
                                                 std::optional<ImageItem> item)
    : id_(id), info_(std::move(info)), item_(std::move(item)), loaded_(info_.has_value()) {}

ImageContainerCompatibilities GelbooruContainerGetter::compatibilities() const noexcept {
	return {};
}

NetworkRequestTask<std::monostate> GelbooruContainerGetter::ensure_loaded(RequestorContext& context) {
	if (loaded_) {
		co_return std::monostate{};
	}

	GetRequest request = { .url = format("{}/index.php", gelbooru::get_api_base(context)) };
	request.url_params.add("page", "dapi");
	request.url_params.add("s",    "post");
	request.url_params.add("q",    "index");
	request.url_params.add("json", "1");
	request.url_params.add("id",   std::to_string(id_));

	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}

	const boost::json::array* posts = gelbooru::posts_of(*json_result);
	if (!posts || posts->empty()) {
		co_return make_response_error(RequestErrorCode::NotFound, "Gelbooru post not found");
	}
	const boost::json::object* post = posts->front().if_object();
	if (!post) {
		co_return make_response_error(RequestErrorCode::NotFound, "Gelbooru post not found");
	}

	info_   = gelbooru::post_to_container_info(*post);
	item_   = gelbooru::post_to_item(*post);
	loaded_ = true;
	co_return std::monostate{};
}

NetworkRequestTask<ImageContainerInfo> GelbooruContainerGetter::info(RequestorContext context) {
	if (auto loaded = co_await ensure_loaded(context); !loaded) {
		co_return unexpected(std::move(loaded.error()));
	}
	co_return *info_;
}

NetworkRequestTask<PageResults<ImageItem>> GelbooruContainerGetter::items(
    RequestorContext context, GetFilters) {
	if (auto loaded = co_await ensure_loaded(context); !loaded) {
		co_return unexpected(std::move(loaded.error()));
	}

	PageResults<ImageItem> page;
	if (item_) {
		page.results.push_back(PageItem<ImageItem>{ .item = *item_, .offset = 0 });
		page.total_count = 1;
		page.next_offset = 1;
	}
	co_return page;
}

NetworkRequestTask<SerializedGetterData> GelbooruContainerGetter::serialize() {
	co_return SerializedGetterData{ .url = std::to_string(id_) };
}

} // namespace aniparse::parsers
