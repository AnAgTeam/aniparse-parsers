/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Headers.hpp"
#include "aniparse/images/Image.hpp"

#include <boost/json.hpp>

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace aniparse {
class RequestorContext;
}

namespace aniparse::parsers::danbooru {

/// Danbooru's public REST host, declared as the parser's mirror set
/// (@see DanbooruParser::mirrors) and consumed through RequestorContext::base_url
/// so a live catalog override can move it without a new binary. A single stable
/// endpoint today, routed the same way as every other source for uniformity.
std::span<const std::string_view> api_hosts();

/// The API base URL for this context: a live catalog override for this parser if
/// present, else the built-in host. Valid while @p context lives. @see api_hosts
std::string_view get_api_base(const RequestorContext& context);

/// The one header the API expects: a descriptive User-Agent (Danbooru asks
/// scripts to identify themselves). No auth — the SFW and NSFW read-path are both
/// anonymous; credentials are a pure rate/limit upgrade the public path omits.
Headers api_headers();

/// The digits of the post id in a Danbooru URL path ("/posts/12345"), if present.
std::optional<ImageContainerID> extract_post_id(std::string_view path);

/// The digits of the pool id in a Danbooru URL path ("/pools/123"), if present.
std::optional<ImageContainerID> extract_pool_id(std::string_view path);

/// Which media kind a post carries, from its file extension: webm/mp4 -> Video,
/// gif/zip(ugoira) -> Animated, everything else -> Still.
ImageItemKind derive_kind(std::string_view file_ext);

/// Map a Danbooru post object to the container's metadata (a post is a
/// container-of-one). Title is synthesized from character/copyright tags, since a
/// booru post has none of its own; revision rides on updated_at.
ImageContainerInfo post_to_container_info(const boost::json::object& post);

/// Map a Danbooru post object to its single media leaf. nullopt when the post has
/// no servable file (banned/deleted posts carry a null file_url) — the caller
/// then yields an empty container. Ugoira (zip) prefers a playable sample variant.
std::optional<ImageItem> post_to_item(const boost::json::object& post);

/// Map a Danbooru pool object (/pools/{id}.json) to container metadata: a pool is a
/// container-of-many (an ordered set of posts). Title is the pool name, total_items
/// its post_count; revision rides on updated_at.
ImageContainerInfo pool_to_container_info(const boost::json::object& pool);

} // namespace aniparse::parsers::danbooru
