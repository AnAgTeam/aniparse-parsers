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

namespace aniparse::parsers::kitsu {

/// Kitsu's public JSON:API host. No auth for reads. (Media assets moved to
/// media.kitsu.app, but the API edge is still served here.)
inline constexpr std::string_view api_host = "https://kitsu.io/api/edge";

/// JSON:API media type — used as both Accept and Content-Type.
inline constexpr std::string_view media_type = "application/vnd.api+json";

/// Headers every call needs: the JSON:API Accept/Content-Type and a User-Agent.
Headers api_headers();

/// A JSON:API single-resource envelope is {"data": {...}}. Pull "data" out as an
/// object, nullptr if the shape does not match.
const boost::json::object* data_object(const boost::json::value& envelope);

/// A JSON:API collection envelope is {"data": [...]}. Pull "data" out as an array.
const boost::json::array* data_array(const boost::json::value& envelope);

/// Total resource count from {"meta":{"count":N}}, 0 if absent.
std::size_t meta_count(const boost::json::value& envelope);

/// The manga ref (numeric id or slug) in a kitsu URL path ("/manga/one-piece",
/// "/manga/38"), if present. Empty optional if the path carries none.
std::optional<std::string> extract_ref(std::string_view path);

/// Map a Kitsu manga resource (id + attributes) to a preview MangaInfo
/// (id/title/poster), returned by preview_info without a detail fetch.
MangaInfo media_to_preview(const boost::json::object& resource);

/// Map a full Kitsu manga resource to MangaInfo. @p included is the envelope's
/// top-level "included" array (categories requested via ?include=categories),
/// folded into tags; pass nullptr when categories were not sideloaded.
MangaInfo media_to_info(const boost::json::object& resource,
                        const boost::json::array* included);

} // namespace aniparse::parsers::kitsu
