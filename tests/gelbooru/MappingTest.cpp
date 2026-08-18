/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "support/ParserTest.hpp"

#include "aniparse/parsers/gelbooru/detail/GelbooruApi.hpp"

#include <boost/json.hpp>

// Deterministic mapping tests over a synthetic Gelbooru DAPI envelope. Drive only
// the pure mapping surface of GelbooruApi.hpp — no network, no credentials — so they
// validate the model fit for a source that differs from Danbooru: a wrapped
// envelope, a single space-separated tag string, and a mandatory media Referer.

namespace gelbooru = aniparse::parsers::gelbooru;
using aniparse::ImageItemKind;
using aniparse::parsertest::load_fixture_json;

TEST_CASE("gelbooru posts_of unwraps the envelope and guards empty results", "[gelbooru]") {
	boost::json::value env = load_fixture_json("gelbooru/fixtures/posts_page.json");
	const boost::json::array* posts = gelbooru::posts_of(env);
	REQUIRE(posts != nullptr);
	CHECK(posts->size() == 2);

	// Zero-result shapes read as "no posts", not an error.
	const boost::json::value missing   = boost::json::parse(R"({"@attributes":{}})");
	const boost::json::value empty_str  = boost::json::parse(R"({"post":""})");
	CHECK(gelbooru::posts_of(missing) == nullptr);
	CHECK(gelbooru::posts_of(empty_str) == nullptr);

	// A bare array (some deployments) is accepted directly.
	const boost::json::value bare = boost::json::parse(R"([{"id":1}])");
	REQUIRE(gelbooru::posts_of(bare) != nullptr);
	CHECK(gelbooru::posts_of(bare)->size() == 1);
}

TEST_CASE("gelbooru post maps tags, uploader, rating and revision", "[gelbooru]") {
	boost::json::value env = load_fixture_json("gelbooru/fixtures/posts_page.json");
	const boost::json::array* posts = gelbooru::posts_of(env);
	REQUIRE(posts);
	aniparse::ImageContainerInfo info =
	    gelbooru::post_to_container_info(posts->at(0).as_object(), "https://gelbooru.com/");

	CHECK(info.id == 8000001);
	CHECK(info.common.title == "#8000001"); // synthesized — a booru post has no title
	// The single space-separated tag string is split into six tokens.
	REQUIRE(info.common.tags.size() == 6);
	CHECK(info.common.tags.front().name == "1girl");
	CHECK(info.common.tags.back().ref == "highres");
	REQUIRE(info.common.uploader.has_value());
	CHECK(info.common.uploader->name == "example_uploader");
	CHECK(info.common.revision == "1750000000"); // "change" timestamp as opaque marker
	CHECK(info.common.is_hentai == false);        // general
	CHECK(info.common.age_restriction == 0);
	REQUIRE(info.common.previews.size() == 1);
	CHECK(info.common.previews.front().headers.get("Referer") == "https://gelbooru.com/");
}

TEST_CASE("gelbooru media leaf carries the Referer; video derives kind and poster", "[gelbooru]") {
	boost::json::value env = load_fixture_json("gelbooru/fixtures/posts_page.json");
	const boost::json::array* posts = gelbooru::posts_of(env);
	REQUIRE(posts);

	auto still = gelbooru::post_to_item(posts->at(0).as_object(), "https://gelbooru.com/");
	REQUIRE(still.has_value());
	CHECK(still->kind == ImageItemKind::Still);
	CHECK(still->image.url == "https://img4.gelbooru.com/images/ab/cd/abcd0001.png");
	CHECK(still->image.headers.get("Referer") == "https://gelbooru.com/"); // hotlink protection
	REQUIRE(still->image.size.has_value());
	CHECK(still->image.size->width == 1200);
	CHECK_FALSE(still->poster.has_value());

	// The Referer is site data, not baked into the mapping: with no referer, none is
	// attached (the discriminating check that the hardcoded host is gone).
	auto still_no_ref = gelbooru::post_to_item(posts->at(0).as_object(), std::nullopt);
	REQUIRE(still_no_ref.has_value());
	CHECK(still_no_ref->image.headers.get("Referer").empty());

	auto video = gelbooru::post_to_item(posts->at(1).as_object(), "https://gelbooru.com/");
	REQUIRE(video.has_value());
	CHECK(video->kind == ImageItemKind::Video); // .webm
	// sample_url is empty, so the preview stands in as the poster.
	REQUIRE(video->poster.has_value());
	CHECK(video->poster->url == "https://img4.gelbooru.com/thumbnails/ef/01/thumbnail_ef010002.jpg");
	CHECK(video->poster->headers.get("Referer") == "https://gelbooru.com/");
}

TEST_CASE("gelbooru derive_kind and extract_id", "[gelbooru]") {
	CHECK(gelbooru::derive_kind("webm") == ImageItemKind::Video);
	CHECK(gelbooru::derive_kind("mp4") == ImageItemKind::Video);
	CHECK(gelbooru::derive_kind("gif") == ImageItemKind::Animated);
	CHECK(gelbooru::derive_kind("png") == ImageItemKind::Still);
	CHECK(gelbooru::derive_kind("jpg") == ImageItemKind::Still);

	// id rides in the query string, not the path.
	CHECK(gelbooru::extract_id("page=post&s=view&id=12345") == 12345);
	CHECK(gelbooru::extract_id("id=678&page=post") == 678);
	CHECK_FALSE(gelbooru::extract_id("page=post&s=list").has_value());
	// A boundary is required, so parent_id=/pool_id= do not match the id= key.
	CHECK_FALSE(gelbooru::extract_id("parent_id=99").has_value());
	CHECK(gelbooru::extract_id("parent_id=99&id=5") == 5);
}
