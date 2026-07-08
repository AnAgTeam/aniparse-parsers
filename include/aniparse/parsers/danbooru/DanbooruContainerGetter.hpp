/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/images/Image.hpp"

#include <optional>

namespace aniparse::parsers {

/**
 * @brief One Danbooru post, as an image container-of-one. Identity is the numeric
 * post id, round-tripped through serialize().
 *
 * Built two ways: from a search/list result the full post is already in hand, so
 * info() and items() are served from the cached mapping with no extra fetch; from
 * a URL or serialized id alone, the first info()/items() call resolves the post
 * via /posts/{id}.json and caches it.
 */
class DanbooruContainerGetter : public ImageContainerGetter {
public:
	/// @param id   The numeric post id — its identity.
	/// @param info Container metadata already mapped from a list result; nullopt
	///             when constructed from an id alone (resolved on first use).
	/// @param item The post's media leaf, mapped alongside @p info; nullopt both
	///             when unresolved and when the post has no servable file.
	explicit DanbooruContainerGetter(ImageContainerID id,
	                                 std::optional<ImageContainerInfo> info = std::nullopt,
	                                 std::optional<ImageItem> item = std::nullopt);

	ImageContainerCompatibilities compatibilities() const noexcept override;

	NetworkRequestTask<ImageContainerInfo> info(RequestorContext context) override;

	NetworkRequestTask<PageResults<ImageItem>> items(
	    RequestorContext context,
	    GetFilters filters) override;

	NetworkRequestTask<SerializedGetterData> serialize() override;

private:
	/// Resolve the post (once) when built from an id alone, caching info_/item_.
	NetworkRequestTask<std::monostate> ensure_loaded(RequestorContext& context);

	ImageContainerID id_;
	std::optional<ImageContainerInfo> info_;
	std::optional<ImageItem> item_;
	/// Whether the post has been resolved — distinguishes "not loaded" from
	/// "loaded, but the post has no media leaf" (both leave item_ empty).
	bool loaded_ = false;
};

} // namespace aniparse::parsers
