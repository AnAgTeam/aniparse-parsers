/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/engines/BooruPoolGetter.hpp"

namespace aniparse::parsers {

/**
 * @brief A Danbooru pool, as an image container-of-many: the generic booru pool getter
 * bound to the Danbooru engine. A thin binding — pool metadata (`/pools/{id}.json`) and
 * pool-order paging (`ordpool:{id}`) live in engines::BooruPoolGetter.
 */
class DanbooruPoolGetter : public engines::BooruPoolGetter {
public:
	explicit DanbooruPoolGetter(ImageContainerID id);
};

} // namespace aniparse::parsers
