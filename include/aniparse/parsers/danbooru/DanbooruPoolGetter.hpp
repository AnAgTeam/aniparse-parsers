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
 * @brief A Danbooru pool, as an image container-of-many: an ordered set of posts
 * (a scanned set, a doujin, ...). Identity is the numeric pool id, round-tripped
 * through serialize() with a "pools/" prefix to distinguish it from a post.
 *
 * info() resolves the pool via /pools/{id}.json (name, description, post_count);
 * items() pages through the pool's posts in order via the `ordpool:{id}` metatag,
 * mapping each to a media leaf — the container-of-many counterpart of a single
 * post's container-of-one.
 */
class DanbooruPoolGetter : public ImageContainerGetter {
public:
	/// @param id The numeric pool id — its identity.
	explicit DanbooruPoolGetter(ImageContainerID id);

	ImageContainerCompatibilities compatibilities() const noexcept override;

	NetworkRequestTask<ImageContainerInfo> info(RequestorContext context) override;

	NetworkRequestTask<PageResults<ImageItem>> items(
	    RequestorContext context,
	    GetFilters filters) override;

	NetworkRequestTask<SerializedGetterData> serialize() override;

private:
	/// Resolve the pool metadata (once), caching info_.
	NetworkRequestTask<std::monostate> ensure_info(RequestorContext& context);

	ImageContainerID id_;
	std::optional<ImageContainerInfo> info_;
};

} // namespace aniparse::parsers
