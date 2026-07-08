/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/ParserStore.hpp"

namespace aniparse::parsers {

/**
 * @brief Register the example parsers (AniList, Kitsu) into a store.
 * The single entry point for consumers of this repo — mirrors the shape a real
 * extensions bundle exposes, so it drops into the same wiring.
 */
extern void emplace_default_parsers(ParserStore& store);

} // namespace aniparse::parsers
