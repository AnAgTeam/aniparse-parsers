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

void emplace_default_parsers(ParserStore::Edit& edit) {
	edit.add_parser(std::make_shared<AniListParser>());
	edit.add_parser(std::make_shared<KitsuParser>());
	edit.add_parser(std::make_shared<DanbooruParser>());
	edit.add_parser(std::make_shared<GelbooruParser>());
	// A self-contained demo source (fictional titles, embedded artwork). Present in
	// every build so the app has a working library/reader with no external source,
	// and so the read path can be exercised offline. @see demo::DemoParser
	edit.add_parser(std::make_shared<demo::DemoParser>());
}

void emplace_default_parsers(ParserStore& store) {
	// One batched edit: every bare store.add_parser() commits its own routing-snapshot
	// rebuild, so registering N parsers one by one costs N full scanner rebuilds.
	auto edit = store.begin_edit();
	emplace_default_parsers(edit);
	edit.commit();
}

} // namespace aniparse::parsers
