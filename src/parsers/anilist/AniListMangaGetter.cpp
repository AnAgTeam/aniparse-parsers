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

#include <cctype>
#include <string>

namespace aniparse::parsers {

namespace {
	/// The Media detail query: everything media_to_info reads. Field names must
	/// match the schema exactly or the whole request errors.
	constexpr std::string_view media_query = R"(query ($id: Int) {
  Media(id: $id, type: MANGA) {
    id
    idMal
    title { romaji english native }
    description(asHtml: true)
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

	/// The relations query. relationType is AniList's own word for the edge (ADAPTATION,
	/// SEQUEL, SIDE_STORY, ...) and node.type says which catalogue the related work lives
	/// in — the two fields RelatedWork is built from. idMal rides along because an
	/// adaptation is an ANIME this parser cannot open: the MAL id is what lets a consumer
	/// find it on a source that serves anime.
	constexpr std::string_view relations_query = R"(query ($id: Int) {
  Media(id: $id, type: MANGA) {
    relations {
      edges {
        relationType
        node {
          id
          idMal
          type
          title { romaji english native }
          coverImage { extraLarge large }
        }
      }
    }
  }
})";

	/// AniList spells its relation types SCREAMING_SNAKE; the model wants the source's
	/// own word, lower-cased ("ADAPTATION" -> "adaptation").
	std::string relation_word(std::string_view type) {
		std::string word;
		word.reserve(type.size());
		for (const char c : type) {
			word += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
		}
		return word;
	}
} // namespace

AniListMangaGetter::AniListMangaGetter(int media_id, std::optional<MangaInfo> preview)
    : media_id_(media_id), preview_(std::move(preview)) {}

MangaGetterCompatibilities AniListMangaGetter::compatibilities() const noexcept {
	return {};
}

// The card the listing handed this getter; nullopt when it was built from a URL
// or a serialized id. No fallback to info(): the caller decides whether the full
// record is worth a request. @see MangaGetter::preview_info
std::optional<MangaInfo> AniListMangaGetter::preview_info() const noexcept {
	return preview_;
}

NetworkRequestTask<MangaInfo> AniListMangaGetter::info(RequestorContext context) const {
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

NetworkRequestTask<PageResults<RelatedWork>> AniListMangaGetter::related(
    RequestorContext context, GetFilters filters) const {
	boost::json::object variables;
	variables["id"] = media_id_;

	PostRequest request = {
	    .url  = std::string(anilist::get_api_base(context)),
	    .body = anilist::graphql_body(relations_query, std::move(variables)),
	};

	auto json_result = co_await context.request_json(request);
	if (!json_result) {
		co_return unexpected(std::move(json_result.error()));
	}
	if (std::string error = anilist::graphql_error(*json_result); !error.empty()) {
		co_return make_response_error(RequestErrorCode::Unknown, error);
	}

	namespace json = aniparse::json;
	const boost::json::object* data  = anilist::graphql_data(*json_result);
	const boost::json::object* media = json::object_field(data, "Media");
	const boost::json::object* relations = json::object_field(media, "relations");
	const boost::json::array*  edges = json::array_field(relations, "edges");
	if (!edges) {
		co_return make_response_error(RequestErrorCode::UnexpectedResponse,
		                              "unexpected relations response shape");
	}

	PageResults<RelatedWork> works;
	for (const boost::json::value& entry : *edges) {
		const boost::json::object* edge = entry.if_object();
		const boost::json::object* node = json::object_field(edge, "node");
		if (!node) {
			continue;
		}
		if (works.results.size() >= filters.limit) {
			break;
		}

		// node.type decides everything downstream: a MANGA relation is a work this
		// parser can open (so it gets a handle), an ANIME relation is one it cannot
		// (so it gets ids instead, and the consumer opens it on an anime source).
		const bool is_anime  = json::str(*node, "type") == "ANIME";
		const MediaKind kind = is_anime ? MediaKind::Anime : MediaKind::Manga;
		const long id        = static_cast<long>(json::integer(*node, "id"));

		// The node carries the same title/cover shape as a search hit, so the existing
		// mapping reads it — only the fields a relation actually has are kept.
		MangaInfo card = anilist::media_to_preview(*node);
		RelatedWork work{
		    .kind     = kind,
		    .relation = relation_word(json::str(*edge, "relationType")),
		    .title    = std::move(card.common.title),
		    .previews = std::move(card.common.previews),
		};
		if (id > 0) {
			work.external_ids.push_back(ExternalId{
			    .ns   = std::string(id_namespaces::anilist),
			    .kind = kind,
			    .id   = std::to_string(id),
			});
		}
		if (long mal = static_cast<long>(json::integer(*node, "idMal")); mal > 0) {
			work.external_ids.push_back(ExternalId{
			    .ns   = std::string(id_namespaces::mal),
			    .kind = kind,
			    .id   = std::to_string(mal),
			});
		}
		// Only manga: this parser serves no anime root getter, so a handle for an
		// adaptation would be one nobody could read back. @see RelatedWork::handle
		if (!is_anime && id > 0) {
			work.handle = SerializedGetterData{ .url = std::to_string(id) };
		}

		works.results.emplace_back(std::move(work), static_cast<pageoff>(works.results.size()));
	}
	works.total_count = works.results.size();
	works.next_offset = works.total_count;
	co_return works;
}

NetworkRequestTask<PageResults<MangaPage>> AniListMangaGetter::chapter_pages(
    RequestorContext, MangaChapterRef, GetFilters, std::optional<MangaTranslationID>) const {
	// AniList is a metadata source: it catalogs manga but hosts no chapter images.
	co_return make_response_error(RequestErrorCode::NotImplemented,
	                              "AniList does not host chapter pages");
}

NetworkRequestTask<SerializedGetterData> AniListMangaGetter::serialize() const {
	// Identity is the numeric media id; the preview is a search-time cache, not
	// identity, so a restored getter fetches info() on preview_info instead.
	co_return SerializedGetterData{
	    .url = std::to_string(media_id_),
	};
}

} // namespace aniparse::parsers
