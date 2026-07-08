/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Headers.hpp"
#include "aniparse/manga/Manga.hpp"

#include <boost/json.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace aniparse::parsers::anilist {

/// AniList's single GraphQL endpoint. Public, no auth needed for reads.
inline constexpr std::string_view api_host = "https://graphql.anilist.co";

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
