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
 * @brief One Gelbooru post, as an image container-of-one. Identity is the numeric
 * post id, round-tripped through serialize().
 *
 * From a search result the post is already mapped, so info()/items() are served
 * from the cache; from an id alone the first call resolves the post via the DAPI
 * post index (s=post&id=<id>) and caches it.
 */
class GelbooruContainerGetter : public ImageContainerGetter {
public:
	explicit GelbooruContainerGetter(ImageContainerID id,
	                                 std::optional<ImageContainerInfo> info = std::nullopt,
	                                 std::optional<ImageItem> item = std::nullopt);

	ImageContainerCompatibilities compatibilities() const noexcept override;

	NetworkRequestTask<ImageContainerInfo> info(RequestorContext context) override;

	NetworkRequestTask<PageResults<ImageItem>> items(
	    RequestorContext context,
	    GetFilters filters) override;

	NetworkRequestTask<SerializedGetterData> serialize() override;

private:
	NetworkRequestTask<std::monostate> ensure_loaded(RequestorContext& context);

	ImageContainerID id_;
	std::optional<ImageContainerInfo> info_;
	std::optional<ImageItem> item_;
	bool loaded_ = false;
};

} // namespace aniparse::parsers
