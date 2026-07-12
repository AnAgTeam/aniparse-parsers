/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/engines/BooruEngine.hpp"
#include "aniparse/engines/BooruSite.hpp"

namespace aniparse::parsers::gelbooru {

/// The shared Gelbooru engine (DAPI `index.php?page=dapi&s=post` dialect: wrapped
/// `{"post":[...]}` envelopes with the documented empty-result quirks, query-addressed
/// ids, `sort:` metatags, a flat tag string, and a mandatory media Referer). No pools.
/// A single stateless instance drives every Gelbooru getter. @see engines::BooruEngine
const engines::BooruEngine& engine();

/// The Gelbooru site descriptor: identity, routing domains and API host bound to
/// @ref engine, plus the static-param credential model. Bound by parser and getters.
const engines::BooruSite& site();

} // namespace aniparse::parsers::gelbooru
