/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/engines/BooruImagesGetter.hpp"

namespace aniparse::parsers {

/**
 * @brief Root images getter for Danbooru: the generic booru images getter bound to the
 * Danbooru engine (REST `/posts.json`). A thin binding — search/latest/suggest and URL
 * routing all live in engines::BooruImagesGetter, parameterized by the Danbooru dialect
 * (bare-array envelopes, `order:` sort metatags, path-addressed ids, pools).
 */
class DanbooruImagesGetter : public engines::BooruImagesGetter {
public:
	DanbooruImagesGetter();
};

} // namespace aniparse::parsers
