/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Headers.hpp"
#include "aniparse/manga/Manga.hpp"

#include <boost/json.hpp>

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace aniparse {
class RequestorContext;
}

namespace aniparse::parsers::anilist {

/// AniList's GraphQL host(s). A single official endpoint today, but declared as
/// the parser's mirror set (@see AniListParser::mirrors) and consumed through
/// RequestorContext::base_url, so if the endpoint ever moves a live catalog
/// override can point at the new host without shipping a new binary.
std::span<const std::string_view> api_hosts();

/// The GraphQL base URL to fetch from for this context: a live catalog override
/// for this parser if present, else the built-in host at the selected alt-link
/// index (clamped). Valid while @p context lives. @see api_hosts
std::string_view get_api_base(const RequestorContext& context);

/// The headers every GraphQL call needs: JSON content-type + accept, and a
/// browser-ish User-Agent (AniList rejects an absent UA).
Headers api_headers();

/// Serialize a GraphQL POST body: {"query": <query>, "variables": <variables>}.
std::string graphql_body(std::string_view query, boost::json::object variables);

/// The GraphQL envelope is {"data": {...}, "errors": [...]}. Pull "data" out as
/// an object, nullptr if absent or not an object.
const boost::json::object* graphql_data(const boost::json::value& envelope);

/// First GraphQL error message, empty if the response carries no non-empty
/// "errors" array. Surfaces a 200-with-errors GraphQL failure (bad id/query).
std::string graphql_error(const boost::json::value& envelope);

/// The numeric media id in an AniList URL path ("/manga/105778/slug"), if any.
std::optional<int> extract_media_id(std::string_view path);

/// Display title of a Media object: english, else romaji, else native.
std::string display_title(const boost::json::object& media);

/// Map a short AniList Media object (id/title/coverImage) to a preview MangaInfo,
/// returned by preview_info without a detail fetch.
MangaInfo media_to_preview(const boost::json::object& media);

/// Map a full AniList Media object to MangaInfo (description/genres/tags/staff/
/// rating/status/chapters/cover).
MangaInfo media_to_info(const boost::json::object& media);

} // namespace aniparse::parsers::anilist
