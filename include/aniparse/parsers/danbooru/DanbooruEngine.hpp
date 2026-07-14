/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/engines/booru/BooruEngine.hpp"
#include "aniparse/engines/booru/BooruSite.hpp"

namespace aniparse::parsers::danbooru {

/// The shared Danbooru engine (REST `/posts.json` dialect: bare-array envelopes,
/// path-addressed ids, `order:` sort metatags, per-category tag mapping, pools via
/// `/pools/{id}.json` + `ordpool:`). A single stateless instance drives every
/// Danbooru getter. @see engines::BooruEngine
const engines::BooruEngine& engine();

/// The Danbooru site descriptor: identity, routing domains and API host bound to
/// @ref engine. The single source both the parser and the getters bind to.
const engines::BooruSite& site();

} // namespace aniparse::parsers::danbooru
