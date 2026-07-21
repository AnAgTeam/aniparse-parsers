/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/DefaultParsers.hpp"

#include "aniparse/parsers/anilist/AniListParser.hpp"
#include "aniparse/parsers/kitsu/KitsuParser.hpp"
#include "aniparse/parsers/danbooru/DanbooruParser.hpp"
#include "aniparse/parsers/gelbooru/GelbooruParser.hpp"
#include "aniparse/parsers/demo/DemoParser.hpp"

#include <memory>

namespace aniparse::parsers {

void emplace_default_parsers(ParserStore& store) {
	store.add_parser(std::make_shared<AniListParser>());
	store.add_parser(std::make_shared<KitsuParser>());
	store.add_parser(std::make_shared<DanbooruParser>());
	store.add_parser(std::make_shared<GelbooruParser>());
	// A self-contained demo source (fictional titles, embedded artwork). Present in
	// every build so the app has a working library/reader with no external source,
	// and so the read path can be exercised offline. @see demo::DemoParser
	store.add_parser(std::make_shared<demo::DemoParser>());
}

} // namespace aniparse::parsers
