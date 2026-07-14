/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/engines/booru/BooruParser.hpp"

namespace aniparse::parsers {

/**
 * @brief Gelbooru (gelbooru.com) — a public, tag-indexed image board. The generic
 * booru parser bound to the Gelbooru site descriptor: unlike Danbooru its structured
 * DAPI is auth-walled, so the descriptor carries a static query-param credential model
 * (user_id + api_key) that BooruParser stamps into the config; media fetches carry a
 * Referer (hotlink protection, applied by the engine's mapping).
 */
class GelbooruParser : public engines::BooruParser {
public:
	GelbooruParser();
};

} // namespace aniparse::parsers
