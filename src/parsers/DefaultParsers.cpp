/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/DefaultParsers.hpp"

#include "aniparse/parsers/anilist/AniListParser.hpp"
#include "aniparse/parsers/kitsu/KitsuParser.hpp"

#include <memory>

namespace aniparse::parsers {

void emplace_default_parsers(ParserStore& store) {
	store.add_parser(std::make_shared<AniListParser>());
	store.add_parser(std::make_shared<KitsuParser>());
}

} // namespace aniparse::parsers
