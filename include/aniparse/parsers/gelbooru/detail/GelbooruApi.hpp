/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/types/Headers.hpp"
#include "aniparse/images/Image.hpp"

#include <boost/json.hpp>

#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace aniparse {
class RequestorContext;
}

namespace aniparse::parsers::gelbooru {

/// Headers the DAPI expects: a descriptive User-Agent. Credentials (api_key +
/// user_id) are NOT headers — they ride as query params in the config, stamped by
/// GelbooruParser::authenticate_context.
Headers api_headers();

/// The numeric id in a Gelbooru post/pool URL query ("...&id=12345"), if present.
/// Gelbooru addresses by query param, not path.
std::optional<ImageContainerID> extract_id(std::string_view query);

/// Which media kind a file url carries, from its extension: webm/mp4 -> Video,
/// gif -> Animated, everything else -> Still.
ImageItemKind derive_kind(std::string_view file_ext);

/// The post array of a DAPI JSON envelope ({ "@attributes":..., "post":[...] }),
/// or nullptr. Guards the documented empty-result quirks: a missing "post" key, a
/// "post":"" string, or a non-array value all read as "no posts".
const boost::json::array* posts_of(const boost::json::value& envelope);

/// Map a Gelbooru post object to the container's metadata (a post is a
/// container-of-one). A booru post has no title of its own, so one is synthesized
/// ("#<id>"); revision rides on the "change" timestamp. @p media_referer, when set,
/// is attached to each preview's fetch headers (Gelbooru hotlink-protects its media).
ImageContainerInfo post_to_container_info(const boost::json::object& post,
                                          std::optional<std::string_view> media_referer);

/// Map a Gelbooru post object to its single media leaf. @p media_referer, when set,
/// is attached to the media (and poster) fetch headers (hotlink protection); a video
/// post takes its same-hash preview as a poster. nullopt when the post exposes no
/// file url.
std::optional<ImageItem> post_to_item(const boost::json::object& post,
                                      std::optional<std::string_view> media_referer);

} // namespace aniparse::parsers::gelbooru
