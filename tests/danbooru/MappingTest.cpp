/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "support/ParserTest.hpp"

#include "aniparse/parsers/danbooru/detail/DanbooruApi.hpp"

#include <boost/json.hpp>

// Deterministic mapping tests over synthetic, schema-faithful Danbooru fixtures
// (see danbooru/fixtures/*.json). These drive only the pure mapping surface of
// DanbooruApi.hpp — no network, no live host — so they gate the exact layer that
// carried the dangling tag-undercount bug and run clean under AddressSanitizer.

namespace danbooru = aniparse::parsers::danbooru;
using aniparse::ImageItemKind;
using aniparse::parsertest::load_fixture_json;

namespace {

boost::json::value load_post(std::string_view name) {
	return load_fixture_json(std::string("danbooru/fixtures/") + std::string(name));
}

} // namespace

TEST_CASE("still post maps every tag category in append order", "[danbooru]") {
	boost::json::value doc = load_post("post_still.json");
	aniparse::ImageContainerInfo info = danbooru::post_to_container_info(doc.as_object());

	// 1 artist + 1 copyright + 1 character + 40 general + 2 meta = 45. The old
	// dangling bug truncated this to ~5, so the exact count is the regression guard.
	REQUIRE(info.tags.size() == 45);

	// Append order is artist, copyright, character, general, meta.
	CHECK(info.tags.front().name == "example_artist");
	CHECK(info.tags[1].name == "touhou_project");
	CHECK(info.tags[2].name == "cirno");
	// A general tag deep in the list — present only if the whole string was walked,
	// not just its first few tokens (the discriminating check vs the truncation bug).
	CHECK(info.tags[42].name == "z_final_general_tag");
	CHECK(info.tags[43].name == "highres");
	CHECK(info.tags.back().name == "absurdres");

	// ref round-trips the raw token (the token-as-key invariant).
	CHECK(info.tags[2].ref == "cirno");
}

TEST_CASE("still post synthesizes title and maps container metadata", "[danbooru]") {
	boost::json::value doc = load_post("post_still.json");
	aniparse::ImageContainerInfo info = danbooru::post_to_container_info(doc.as_object());

	CHECK(info.id == 5000001);
	CHECK(info.title == "cirno (touhou_project)");
	REQUIRE(info.series.has_value());
	CHECK(info.series->name == "touhou_project");
	CHECK(info.revision == "2026-01-02T03:04:05.678-05:00");
	REQUIRE(info.total_items.has_value());
	CHECK(*info.total_items == 1);
	REQUIRE(info.previews.size() == 1);
	CHECK(info.is_hentai == false);
	CHECK(info.age_restriction == 0);
}

TEST_CASE("still post maps its single media leaf", "[danbooru]") {
	boost::json::value doc = load_post("post_still.json");
	std::optional<aniparse::ImageItem> item = danbooru::post_to_item(doc.as_object());

	REQUIRE(item.has_value());
	CHECK(item->kind == ImageItemKind::Still);
	CHECK(item->image.url == "https://cdn.example-booru.test/data/still_5000001.png");
	REQUIRE(item->image.size.has_value());
	CHECK(item->image.size->width == 1200);
	CHECK(item->image.size->height == 1600);
	// A still needs no poster and cdn.donmai.us serves a bare GET (no Referer).
	CHECK_FALSE(item->poster.has_value());
	CHECK(item->image.headers.empty());
}

TEST_CASE("webm post is Video with a poster", "[danbooru]") {
	boost::json::value doc = load_post("post_video.json");
	std::optional<aniparse::ImageItem> item = danbooru::post_to_item(doc.as_object());

	REQUIRE(item.has_value());
	CHECK(item->kind == ImageItemKind::Video);
	REQUIRE(item->poster.has_value());
	CHECK(item->poster->url == "https://cdn.example-booru.test/data/preview/clip_5000002.jpg");
}

TEST_CASE("ugoira swaps the zip for a playable sample variant", "[danbooru]") {
	boost::json::value doc = load_post("post_ugoira.json");
	std::optional<aniparse::ImageItem> item = danbooru::post_to_item(doc.as_object());

	REQUIRE(item.has_value());
	// zip alone derives to Animated; the sample-variant swap upgrades it to Video.
	CHECK(item->kind == ImageItemKind::Video);
	CHECK(item->image.url == "https://cdn.example-booru.test/data/sample/ugoira_5000003.webm");
}

TEST_CASE("banned post yields metadata but no media leaf", "[danbooru]") {
	boost::json::value doc = load_post("post_banned.json");

	// A null file_url means no servable file -> no media item.
	CHECK_FALSE(danbooru::post_to_item(doc.as_object()).has_value());

	// The surviving metadata still maps (container-of-one keeps its info).
	aniparse::ImageContainerInfo info = danbooru::post_to_container_info(doc.as_object());
	CHECK(info.id == 5000004);
	CHECK(info.title == "cirno (original)");
	CHECK_FALSE(info.tags.empty());
}

TEST_CASE("derive_kind maps file extensions to media kinds", "[danbooru]") {
	CHECK(danbooru::derive_kind("webm") == ImageItemKind::Video);
	CHECK(danbooru::derive_kind("mp4") == ImageItemKind::Video);
	CHECK(danbooru::derive_kind("gif") == ImageItemKind::Animated);
	CHECK(danbooru::derive_kind("zip") == ImageItemKind::Animated);
	CHECK(danbooru::derive_kind("png") == ImageItemKind::Still);
	CHECK(danbooru::derive_kind("jpg") == ImageItemKind::Still);
	CHECK(danbooru::derive_kind("webp") == ImageItemKind::Still);
}

TEST_CASE("rating drives the hentai marker and age restriction", "[danbooru]") {
	// A one-field object — exercises the rating branch with no media, no content.
	auto info_for = [](const char* rating) {
		boost::json::object post;
		post["rating"] = rating;
		return danbooru::post_to_container_info(post);
	};
	CHECK(info_for("g").is_hentai == false);
	CHECK(info_for("g").age_restriction == 0);
	CHECK(info_for("s").age_restriction == 0);
	CHECK(info_for("q").age_restriction == 18);
	CHECK(info_for("q").is_hentai == false);
	CHECK(info_for("e").is_hentai == true);
	CHECK(info_for("e").age_restriction == 18);
}

TEST_CASE("extract_post_id reads the digits after /posts/", "[danbooru]") {
	CHECK(danbooru::extract_post_id("/posts/12345") == 12345);
	CHECK(danbooru::extract_post_id("/posts/12345/some-slug") == 12345);
	CHECK(danbooru::extract_post_id("https://danbooru.donmai.us/posts/678?q=x") == 678);
	CHECK_FALSE(danbooru::extract_post_id("/artists/9").has_value());
	CHECK_FALSE(danbooru::extract_post_id("/posts/").has_value());
	CHECK_FALSE(danbooru::extract_post_id("/posts/abc").has_value());
}
