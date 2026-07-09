/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/kitsu/KitsuMangaGetter.hpp"
#include "aniparse/parsers/kitsu/detail/KitsuApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/utility/Coroutines.hpp"
#include "aniparse/utility/Format.hpp"
#include "aniparse/utility/UrlPath.hpp"

#include <boost/json.hpp>

namespace aniparse::parsers {

KitsuMangaGetter::KitsuMangaGetter(std::string ref, std::optional<MangaInfo> preview)
    : ref_(std::move(ref)), preview_(std::move(preview)) {}

MangaGetterCompatibilities KitsuMangaGetter::compatibilities() const noexcept {
	return {};
}

NetworkRequestTask<MangaInfo> KitsuMangaGetter::preview_info(RequestorContext context) {
	if (preview_) {
		co_return *preview_;
	}
	co_return co_await info(context);
}

NetworkRequestTask<MangaInfo> KitsuMangaGetter::info(RequestorContext context) {
	// A numeric ref addresses the resource directly (/manga/{id}); a slug is
	// resolved through the collection filter (/manga?filter[slug]=). Categories
	// are sideloaded so tags come back in one round-trip.
	const bool numeric = all_digits(ref_);
	GetRequest request = {
	    .url = numeric ? format("{}/manga/{}", kitsu::get_api_base(context), ref_)
	                   : format("{}/manga", kitsu::get_api_base(context)),
	};
	request.url_params.add("include", "categories");
	if (!numeric) {
		request.url_params.add("filter[slug]", ref_);
	}

	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}

	// A numeric fetch yields a single-resource {"data":{...}}; a slug fetch a
	// collection {"data":[...]} — take the first match.
	const boost::json::object* resource = nullptr;
	if (numeric) {
		resource = kitsu::data_object(*json_result);
	} else if (const boost::json::array* items = kitsu::data_array(*json_result); items && !items->empty()) {
		resource = items->front().if_object();
	}
	if (!resource) {
		co_return make_response_error(RequestErrorCode::NotFound, "Kitsu manga not found");
	}

	const boost::json::array* included =
	    aniparse::json::array_field(json_result->if_object(), "included");
	co_return kitsu::media_to_info(*resource, included);
}

NetworkRequestTask<PageResults<MangaPage>> KitsuMangaGetter::chapter_pages(
    RequestorContext, MangaChapterRef, GetFilters, std::optional<MangaTranslationID>) {
	// Kitsu is a metadata source: it catalogs manga but hosts no chapter images.
	co_return make_response_error(RequestErrorCode::NotImplemented,
	                              "Kitsu does not host chapter pages");
}

NetworkRequestTask<SerializedGetterData> KitsuMangaGetter::serialize() {
	// Identity is the id/slug ref; the preview is a search-time cache, not
	// identity, so a restored getter fetches info() on preview_info instead.
	co_return SerializedGetterData{
	    .url = ref_,
	};
}

} // namespace aniparse::parsers
