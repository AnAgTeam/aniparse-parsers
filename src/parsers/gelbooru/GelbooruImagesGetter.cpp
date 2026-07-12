/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/gelbooru/GelbooruImagesGetter.hpp"
#include "aniparse/parsers/gelbooru/GelbooruEngine.hpp"

namespace aniparse::parsers {

GelbooruImagesGetter::GelbooruImagesGetter() : engines::BooruImagesGetter(gelbooru::site()) {}

} // namespace aniparse::parsers
