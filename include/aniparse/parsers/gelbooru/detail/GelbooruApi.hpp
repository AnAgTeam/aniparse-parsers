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

namespace aniparse::parsers::gelbooru {

/// Gelbooru's public DAPI host, declared as the parser's mirror set and consumed
/// through RequestorContext::base_url. Single CGI entrypoint (index.php); every
/// operation is query params on it. @see get_api_base
std::span<const std::string_view> api_hosts();

/// The API base URL for this context (catalog override if present, else builtin).
std::string_view get_api_base(const RequestorContext& context);

/// Headers the DAPI expects: a descriptive User-Agent. Credentials (api_key +
/// user_id) are NOT headers — they ride as query params in the config, stamped by
/// GelbooruParser::authenticate_context. @see media_headers
Headers api_headers();

/// Headers a media fetch requires: img*.gelbooru.com 302-redirects a bare GET to a
/// hotlink interstitial; a Referer of gelbooru.com returns the real bytes. Every
/// ImageItem.image.headers carries this.
Headers media_headers();

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
/// ("#<id>"); revision rides on the "change" timestamp.
ImageContainerInfo post_to_container_info(const boost::json::object& post);

/// Map a Gelbooru post object to its single media leaf. Carries the mandatory
/// Referer header; a video post takes its same-hash preview as a poster. nullopt
/// when the post exposes no file url.
std::optional<ImageItem> post_to_item(const boost::json::object& post);

} // namespace aniparse::parsers::gelbooru
