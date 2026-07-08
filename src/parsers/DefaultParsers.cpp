/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/DefaultParsers.hpp"

#include "aniparse/parsers/anilist/AniListParser.hpp"
#include "aniparse/parsers/kitsu/KitsuParser.hpp"
#include "aniparse/parsers/danbooru/DanbooruParser.hpp"

#include <memory>

namespace aniparse::parsers {

void emplace_default_parsers(ParserStore& store) {
	store.add_parser(std::make_shared<AniListParser>());
	store.add_parser(std::make_shared<KitsuParser>());
	store.add_parser(std::make_shared<DanbooruParser>());
}

} // namespace aniparse::parsers
