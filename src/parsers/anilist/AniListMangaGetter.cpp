/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/anilist/AniListMangaGetter.hpp"
#include "aniparse/parsers/anilist/detail/AniListApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/utility/Coroutines.hpp"

#include <boost/json.hpp>

namespace aniparse::parsers {

namespace {
	/// The Media detail query: everything media_to_info reads. Field names must
	/// match the schema exactly or the whole request errors.
	constexpr std::string_view media_query = R"(query ($id: Int) {
  Media(id: $id, type: MANGA) {
    id
    idMal
    title { romaji english native }
    description(asHtml: false)
    coverImage { extraLarge large }
    averageScore
    genres
    tags { name }
    staff { edges { role node { name { full } } } }
    status
    chapters
    isAdult
  }
})";
} // namespace

AniListMangaGetter::AniListMangaGetter(int media_id, std::optional<MangaInfo> preview)
    : media_id_(media_id), preview_(std::move(preview)) {}

MangaGetterCompatibilities AniListMangaGetter::compatibilities() const noexcept {
	return {};
}

NetworkRequestTask<MangaInfo> AniListMangaGetter::preview_info(RequestorContext context) {
	// Short-card info captured at search time; only fall back to the full detail
	// fetch when the getter was built straight from a URL.
	if (preview_) {
		co_return *preview_;
	}
	co_return co_await info(context);
}

NetworkRequestTask<MangaInfo> AniListMangaGetter::info(RequestorContext context) {
	boost::json::object variables;
	variables["id"] = media_id_;

	PostRequest request = {
	    .url  = std::string(anilist::get_api_base(context)),
	    .body = anilist::graphql_body(media_query, std::move(variables)),
	};

	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	if (std::string error = anilist::graphql_error(*json_result); !error.empty()) {
		co_return make_response_error(RequestErrorCode::Unknown, error);
	}

	const boost::json::object* data  = anilist::graphql_data(*json_result);
	const boost::json::object* media = aniparse::json::object_field(data, "Media");
	if (!media) {
		co_return make_response_error(RequestErrorCode::Unknown, "unexpected Media response shape");
	}
	co_return anilist::media_to_info(*media);
}

NetworkRequestTask<PageResults<MangaPage>> AniListMangaGetter::chapter_pages(
    RequestorContext, MangaChapterRef, GetFilters, std::optional<MangaTranslationID>) {
	// AniList is a metadata source: it catalogs manga but hosts no chapter images.
	co_return make_response_error(RequestErrorCode::NotImplemented,
	                              "AniList does not host chapter pages");
}

NetworkRequestTask<SerializedGetterData> AniListMangaGetter::serialize() {
	// Identity is the numeric media id; the preview is a search-time cache, not
	// identity, so a restored getter fetches info() on preview_info instead.
	co_return SerializedGetterData{
	    .url = std::to_string(media_id_),
	};
}

} // namespace aniparse::parsers
