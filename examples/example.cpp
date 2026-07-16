/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 *
 * Live demo of the example parsers against their real public APIs: registers
 * AniList + Kitsu, searches each, prints the top result's details, and shows URL
 * routing. Both are public metadata sources — they catalog manga but host no
 * chapter content, so the reading path is intentionally unimplemented.
 * Needs network reachability to graphql.anilist.co and kitsu.io.
 */
#define _CRT_SECURE_NO_WARNINGS // std::getenv for the Gelbooru credentials demo

#include <aniparse/AniParse.hpp>
#include <aniparse/ParserStore.hpp>
#include <aniparse/net/Client.hpp>
#include <aniparse/parsers/DefaultParsers.hpp>
#include <aniparse/types/Authentication.hpp>
#include <aniparse/utility/Format.hpp>
#include <coro/task.hpp>
#include <coro/sync_wait.hpp>

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <print>
#include <string>
#include <utility>

using namespace aniparse;

static std::string error_line(const RequestError& error) {
	return fmt::format("[{}] {}{}",
	              static_cast<int>(error.code),
	              error.message,
	              error.http_status ? fmt::format(" (http {})", *error.http_status) : std::string{});
}

static void print_info(const MangaInfo& info) {
	std::println("  Title:    {}", info.title);
	std::println("  Original: {}", info.original_title.value_or("-"));
	std::println("  Status:   {}", info.status.name.empty() ? "-" : info.status.name);
	std::println("  Rating:   {}", info.rating ? std::to_string(info.rating->score()) : "-");
	std::println("  Tags:     {}", info.tags.size());
	std::println("  Cover:    {}", info.previews.empty() ? "-" : info.previews.front().url);
	const std::string& description = info.description.text;
	std::println("  Summary:  {}", description.substr(0, std::min<std::size_t>(description.size(), 100)));
}

static std::string_view kind_name(ImageItemKind kind) {
	switch (kind) {
	case ImageItemKind::Still:    return "still";
	case ImageItemKind::Animated: return "animated";
	case ImageItemKind::Video:    return "video";
	}
	return "?";
}

// Search an image source by tags and show the top posts + the first post's media.
static coro::task<void> showcase_images(ParserStore& store, RequestorContext base,
                                        std::string key, std::string tags) {
	auto parser = store.find_by_key(key);
	if (!parser) {
		std::println("[{}] not registered", key);
		co_return;
	}
	RequestorContext context = base.new_with_config(parser->make_config(base.config()));
	auto root = parser->images_getter();

	std::println("== {} : tags '{}' ==", key, tags);
	SearchRequestQuery search_query;
	search_query.query = tags;
	GetFilters filters{ .from = 0, .limit = 3 };
	auto found = co_await root->search(context, search_query, filters);
	if (!found) {
		std::println("  search failed: {}\n", error_line(found.error()));
		co_return;
	}
	for (const auto& entry : found->results) {
		if (auto info = co_await entry.item->info(context)) {
			std::println("   - {} [{} tags]", info->title, info->tags.size());
		}
	}
	if (!found->results.empty()) {
		std::println("  -- media of top hit --");
		if (auto items = co_await found->results.front().item->items(context, { .from = 0 })) {
			for (const auto& page_item : items->results) {
				const ImageItem& media = page_item.item;
				std::string size = media.image.size
				    ? fmt::format("{}x{}", media.image.size->width, media.image.size->height)
				    : "?";
				std::println("   [{}] {} ({})", kind_name(media.kind), media.image.url, size);
			}
		} else {
			std::println("  items failed: {}", error_line(items.error()));
		}
	}
	std::println("");
}

// Gelbooru's DAPI is credentialed: a live search needs a user-supplied api_key +
// user_id (GELBOORU_USER_ID / GELBOORU_API_KEY in the environment). Autocomplete is
// the one credential-free surface, so it always runs; search only when a key is set.
static coro::task<void> showcase_gelbooru(ParserStore& store, RequestorContext base) {
	auto parser = store.find_by_key("Gelbooru");
	if (!parser) {
		std::println("[Gelbooru] not registered");
		co_return;
	}
	RequestorContext context = base.new_with_config(parser->make_config(base.config()));
	auto root = parser->images_getter();

	std::println("== Gelbooru ==");

	// Credential-free autocomplete: something to see without a key.
	if (auto suggestions = co_await root->suggest(context, "cat", std::nullopt)) {
		std::print("  autocomplete 'cat':");
		for (const auto& suggestion : *suggestions) {
			std::print(" {}", suggestion.value);
		}
		std::println("");
	}

	const char* user_id = std::getenv("GELBOORU_USER_ID");
	const char* api_key = std::getenv("GELBOORU_API_KEY");
	if (!user_id || !*user_id || !api_key || !*api_key) {
		std::println("  set GELBOORU_USER_ID + GELBOORU_API_KEY to run a live search (the DAPI is auth-walled)\n");
		co_return;
	}

	// Fold the credentials into the config; every request then carries them.
	auto authed = co_await parser->authenticate_context(
	    context, AuthenticationUserPassword{ .username = user_id, .password = api_key });
	if (!authed) {
		std::println("  auth failed: {}\n", error_line(authed.error()));
		co_return;
	}
	context = context.new_with_config(std::make_shared<ParserConfig>(**authed));

	SearchRequestQuery query;
	query.query = "cirno rating:general";
	auto found = co_await root->search(context, query, GetFilters{ .from = 0, .limit = 3 });
	if (!found) {
		std::println("  search failed: {}\n", error_line(found.error()));
		co_return;
	}
	for (const auto& entry : found->results) {
		if (auto info = co_await entry.item->info(context)) {
			std::println("   - {} [{} tags]", info->title, info->tags.size());
		}
	}
	if (!found->results.empty()) {
		std::println("  -- media of top hit --");
		if (auto items = co_await found->results.front().item->items(context, { .from = 0 })) {
			for (const auto& page_item : items->results) {
				const ImageItem& media = page_item.item;
				std::println("   [{}] {}", kind_name(media.kind), media.image.url);
			}
		}
	}
	std::println("");
}

// Search a single source by key and show the top result's full details.
static coro::task<void> showcase(ParserStore& store, RequestorContext base,
                                 std::string key, std::string query) {
	auto parser = store.find_by_key(key);
	if (!parser) {
		std::println("[{}] not registered", key);
		co_return;
	}
	// Ready a context for this source: its config carries the parser id + the
	// per-source request headers (see Parser::make_config).
	RequestorContext context = base.new_with_config(parser->make_config(base.config()));
	auto root = parser->mangas_getter();

	std::println("== {} : search '{}' ==", key, query);
	SearchRequestQuery search_query;
	search_query.query = query;
	GetFilters filters{ .from = 0, .limit = 3 };
	auto found = co_await root->search(context, search_query, filters);
	if (!found) {
		std::println("  search failed: {}\n", error_line(found.error()));
		co_return;
	}
	std::println("  total matches: {}", found->total_count);
	for (const auto& item : found->results) {
		if (auto preview = co_await item.item->preview_info(context)) {
			std::println("   - {} / {}", preview->title, preview->original_title.value_or("-"));
		}
	}
	if (!found->results.empty()) {
		std::println("  -- details of top hit --");
		if (auto info = co_await found->results.front().item->info(context)) {
			print_info(*info);
		} else {
			std::println("  info failed: {}", error_line(info.error()));
		}
	}
	std::println("");
}

// Identity lookup: the cross-source anchor in action. A consumer that holds only a
// foreign id — a MyAnimeList manga id, as another source would have emitted it in
// MangaInfo::external_ids — resolves it to this source's item, then reads back the
// ids that item carries. That round trip is what lets one work be recognised across
// parsers instead of being a fresh, unrelated entry in every source.
//
// Deliberately discriminating: were the filter silently dropped, the query would
// degenerate into the default popularity page — an obviously different title, not a
// plausible near-miss.
static coro::task<void> showcase_identity(ParserStore& store, RequestorContext base,
                                          std::string mal_id) {
	auto parser = store.find_by_key("AniList");
	if (!parser) {
		std::println("[AniList] not registered");
		co_return;
	}
	RequestorContext context = base.new_with_config(parser->make_config(base.config()));
	auto root = parser->mangas_getter();

	std::println("== AniList : lookup by MyAnimeList manga id {} ==", mal_id);
	SearchRequestQuery query;
	query.filters.emplace(
	    std::string(search_keys::mal_id),
	    ItemSelection{ { mal_id, ItemSelectionValue{ .name = mal_id, .exclusive = false } } });

	auto found = co_await root->search(context, query, GetFilters{ .from = 0, .limit = 3 });
	if (!found) {
		std::println("  lookup failed: {}\n", error_line(found.error()));
		co_return;
	}
	std::println("  matches: {}", found->results.size());
	for (const auto& entry : found->results) {
		auto info = co_await entry.item->info(context);
		if (!info) {
			continue;
		}
		std::println("   - {}", info->title);
		for (const ExternalId& id : info->external_ids) {
			std::println("     carries {} {} = {}",
			             id.kind == MediaKind::Manga ? "manga" : "anime", id.ns, id.id);
		}
	}
	std::println("");
}

// Route a URL to its parser and fetch info, no source key needed up front.
static coro::task<void> route_and_show(ParserStore& store, RequestorContext base, std::string url) {
	std::println("== route '{}' ==", url);
	auto route = store.route_url(url);
	if (!route) {
		std::println("  no parser claims this URL\n");
		co_return;
	}
	RequestorContext context = base.new_with_config(route->parser->make_config(base.config()));
	std::println("  routed to '{}'", route->parser->info().name);
	auto getter = co_await route->parser->mangas_getter()->parse_url(context, std::move(route->url));
	if (!getter) {
		std::println("  parse_url failed: {}\n", error_line(getter.error()));
		co_return;
	}
	if (auto info = co_await (*getter)->info(context)) {
		print_info(*info);
	} else {
		std::println("  info failed: {}", error_line(info.error()));
	}
	std::println("");
}

coro::task<void> demo() {
	RequestorContext context(std::make_shared<AsyncClient>(), nullptr, nullptr);

	ParserStore store;
	parsers::emplace_default_parsers(store);

	co_await showcase(store, context, "AniList", "chainsaw man");
	co_await showcase(store, context, "Kitsu",   "berserk");

	// Danbooru is an image source (read-path): search by SFW tags, show media +
	// its kind. rating:general keeps it safe and is exempt from the 2-tag limit.
	co_await showcase_images(store, context, "Danbooru", "cirno rating:general");

	// Gelbooru: autocomplete always; live search when GELBOORU_USER_ID + _API_KEY are set.
	co_await showcase_gelbooru(store, context);

	// Frieren's MyAnimeList *manga* id — its anime is a different entry with an
	// unrelated id, which is why an ExternalId names the catalogue it addresses.
	co_await showcase_identity(store, context, "126287");

	co_await route_and_show(store, context, "https://anilist.co/manga/30013/One-Piece");
	co_await route_and_show(store, context, "https://kitsu.io/manga/one-piece");

	co_return;
}

int main() {
	coro::sync_wait(demo());
}
