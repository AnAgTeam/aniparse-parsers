/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/danbooru/DanbooruPoolGetter.hpp"
#include "aniparse/parsers/danbooru/DanbooruEngine.hpp"

namespace aniparse::parsers {

DanbooruPoolGetter::DanbooruPoolGetter(ImageContainerID id)
    : engines::BooruPoolGetter(danbooru::site(), id) {}

} // namespace aniparse::parsers
