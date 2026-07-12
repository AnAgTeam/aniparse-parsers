/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/engines/BooruImagesGetter.hpp"

namespace aniparse::parsers {

/**
 * @brief Root images getter for Gelbooru: the generic booru images getter bound to the
 * Gelbooru engine (DAPI post index). A thin binding — search/latest/suggest and URL
 * routing all live in engines::BooruImagesGetter, parameterized by the Gelbooru dialect
 * (wrapped envelopes, `sort:` metatags, query-addressed ids, a mandatory media Referer).
 */
class GelbooruImagesGetter : public engines::BooruImagesGetter {
public:
	GelbooruImagesGetter();
};

} // namespace aniparse::parsers
