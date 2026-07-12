/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/danbooru/DanbooruImagesGetter.hpp"
#include "aniparse/parsers/danbooru/DanbooruEngine.hpp"

namespace aniparse::parsers {

DanbooruImagesGetter::DanbooruImagesGetter() : engines::BooruImagesGetter(danbooru::site()) {}

} // namespace aniparse::parsers
