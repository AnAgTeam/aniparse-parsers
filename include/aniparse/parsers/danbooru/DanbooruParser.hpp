/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/engines/BooruParser.hpp"

namespace aniparse::parsers {

/**
 * @brief Danbooru (danbooru.donmai.us) — a public, tag-indexed image board. The
 * generic booru parser bound to the Danbooru site descriptor: a documented, no-auth
 * REST API (`/posts.json`) that both catalogs and serves media; each post is a
 * container-of-one, pools are containers-of-many, search is by tags.
 */
class DanbooruParser : public engines::BooruParser {
public:
	DanbooruParser();
};

} // namespace aniparse::parsers
