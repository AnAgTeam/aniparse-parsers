/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>

namespace aniparse::parsers::demo {

/**
 * @brief The bytes of one embedded demo asset, addressed by its key.
 *
 * The demo source ships its own artwork compiled into the library (see
 * tools/embed_demo_assets.py), so it needs no filesystem and no network. A key is
 * the path under src/parsers/demo/assets, e.g. "covers/fox-lantern.png" or
 * "ch1/p01.png" — the same value the demo parser puts after the demo:// scheme.
 * @param key Asset key (path under the assets root).
 * @return The bytes, borrowed from static storage; nullopt when no asset matches.
 */
[[nodiscard]] std::optional<std::span<const unsigned char>> asset_bytes(std::string_view key);

} // namespace aniparse::parsers::demo
