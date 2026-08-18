/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/ParserStore.hpp"

namespace aniparse::parsers {

/**
 * @brief Register the bundled parsers into a store, in one batched edit.
 * The single entry point for consumers of this repo — mirrors the shape a real
 * extensions bundle exposes, so it drops into the same wiring.
 *
 * A bare ParserStore::add_parser() commits its own routing-snapshot rebuild, so the
 * whole set goes through a single ParserStore::Edit and one commit.
 * @param store The store to populate
 * @throw std::logic_error On a parser identifier conflict
 */
extern void emplace_default_parsers(ParserStore& store);

/**
 * @brief Add the bundled parsers to an already-open store edit, without committing.
 * For a caller that batches this set together with more parsers (e.g. a downstream
 * extension seam) so the whole registration commits — and rebuilds the routing
 * snapshot — exactly once.
 * @param edit An open edit from ParserStore::begin_edit(); the caller commits it
 * @throw std::logic_error On a parser identifier conflict
 */
extern void emplace_default_parsers(ParserStore::Edit& edit);

} // namespace aniparse::parsers
