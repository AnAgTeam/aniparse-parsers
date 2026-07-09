/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/danbooru/detail/DanbooruApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/UrlPath.hpp"

#include <boost/json.hpp>

#include <array>

namespace aniparse::parsers::danbooru {

namespace {
	namespace json = aniparse::json;

	/// The five tag-category fields Danbooru pre-splits a post's tag string into.
	/// Every entry becomes a Tag whose ref is the raw underscored token that
	/// round-trips straight back into a `tags=` search (the token-as-key invariant).
	void append_tags(std::vector<Tag>& tags, const boost::json::object& post,
	                 std::string_view field) {
		// Own the string: json::str returns by value, so a string_view over it
		// would dangle the moment this expression ends (Json.hpp's temporary caveat).
		std::string names = json::str(post, field);
		std::size_t start = 0;
		while (start < names.size()) {
			std::size_t end = names.find(' ', start);
			if (end == std::string_view::npos) {
				end = names.size();
			}
			if (end > start) {
				std::string token(names.substr(start, end - start));
				tags.push_back(Tag{ .name = token, .ref = token });
			}
			start = end + 1;
		}
	}

	/// A booru post has no title of its own; synthesize one from the character and
	/// copyright tags ("hatsune_miku (vocaloid)"), falling back to "#<id>".
	std::string synthesize_title(const boost::json::object& post, ImageContainerID id) {
		std::string character = json::str(post, "tag_string_character");
		std::string copyright = json::str(post, "tag_string_copyright");
		// Keep only the first tag of each field for a short, readable label.
		auto first_token = [](std::string_view s) {
			std::size_t space = s.find(' ');
			return std::string(space == std::string_view::npos ? s : s.substr(0, space));
		};
		std::string title;
		if (!character.empty()) {
			title = first_token(character);
		}
		if (!copyright.empty()) {
			std::string series = first_token(copyright);
			title = title.empty() ? series : title + " (" + series + ")";
		}
		if (title.empty()) {
			title = "#" + std::to_string(id);
		}
		return title;
	}

	/// A media_asset.variants[] entry with the given file type, if the post has one
	/// — used to pick a playable url for ugoira (whose top-level file_url is a zip).
	std::string variant_url(const boost::json::object& post, std::string_view type) {
		const boost::json::object* asset = json::object_field(post, "media_asset");
		const boost::json::array* variants = json::array_field(asset, "variants");
		if (!variants) {
			return {};
		}
		for (const boost::json::value& entry : *variants) {
			const boost::json::object* variant = entry.if_object();
			if (variant && json::str(variant, "type") == type) {
				return json::str(variant, "url");
			}
		}
		return {};
	}
} // namespace

std::span<const std::string_view> api_hosts() {
	using namespace std::string_view_literals;
	static constexpr std::array hosts = { "https://danbooru.donmai.us"sv };
	return hosts;
}

std::string_view get_api_base(const RequestorContext& context) {
	return context.base_url(api_hosts());
}

Headers api_headers() {
	return Headers{
	    { "User-Agent", "aniparse-parsers/0.1 (+https://github.com/aniparse)" },
	};
}

std::optional<ImageContainerID> extract_post_id(std::string_view path) {
	if (auto id = numeric_after(path, "/posts/")) {
		return static_cast<ImageContainerID>(*id);
	}
	return std::nullopt;
}

std::optional<ImageContainerID> extract_pool_id(std::string_view path) {
	if (auto id = numeric_after(path, "/pools/")) {
		return static_cast<ImageContainerID>(*id);
	}
	return std::nullopt;
}

ImageItemKind derive_kind(std::string_view file_ext) {
	if (file_ext == "webm" || file_ext == "mp4") {
		return ImageItemKind::Video;
	}
	// zip = ugoira: a frame archive, animated but not directly playable.
	if (file_ext == "gif" || file_ext == "zip") {
		return ImageItemKind::Animated;
	}
	return ImageItemKind::Still;
}

ImageContainerInfo post_to_container_info(const boost::json::object& post) {
	ImageContainerInfo info;
	info.id = static_cast<ImageContainerID>(json::integer(post, "id"));
	info.title = synthesize_title(post, info.id);

	append_tags(info.tags, post, "tag_string_artist");
	append_tags(info.tags, post, "tag_string_copyright");
	append_tags(info.tags, post, "tag_string_character");
	append_tags(info.tags, post, "tag_string_general");
	append_tags(info.tags, post, "tag_string_meta");

	if (std::string copyright = json::str(post, "tag_string_copyright"); !copyright.empty()) {
		std::size_t space = copyright.find(' ');
		std::string first = space == std::string::npos ? copyright : copyright.substr(0, space);
		info.series = Series{ .name = first, .ref = first };
	}

	if (std::string preview = json::str(post, "preview_file_url"); !preview.empty()) {
		info.previews.push_back(Image{ .url = std::move(preview) });
	}

	// Opaque change marker: the post's last-updated stamp. Compared only for
	// equality, so the raw string (tz + millisecond fraction) rides as-is.
	info.revision = json::str(post, "updated_at");

	// Danbooru rating is g/s/q/e; explicit is the source's own hentai marker.
	std::string rating = json::str(post, "rating");
	info.is_hentai = rating == "e";
	if (rating == "q" || rating == "e") {
		info.age_restriction = 18;
	}

	// A post is a container-of-one.
	info.total_items = 1;
	return info;
}

std::optional<ImageItem> post_to_item(const boost::json::object& post) {
	std::string file_url = json::str(post, "file_url");
	std::string file_ext = json::str(post, "file_ext");

	// Banned/deleted posts carry no servable file — no media leaf to yield.
	if (file_url.empty()) {
		return std::nullopt;
	}

	ImageItemKind kind = derive_kind(file_ext);

	// Ugoira's top-level file is a zip of frames; swap in a playable sample
	// (webm) variant when the API exposes one so the leaf is directly renderable.
	if (file_ext == "zip") {
		if (std::string sample = variant_url(post, "sample"); !sample.empty()) {
			file_url = std::move(sample);
			kind = ImageItemKind::Video;
		}
	}

	ImageItem item;
	item.image.url = std::move(file_url);
	item.kind = kind;

	int width  = static_cast<int>(json::integer(post, "image_width"));
	int height = static_cast<int>(json::integer(post, "image_height"));
	if (width > 0 && height > 0) {
		item.image.size = ImageResolution{ .width = width, .height = height };
	}

	// A thumbnail distinct from the media, useful as a poster for non-stills.
	if (kind != ImageItemKind::Still) {
		if (std::string preview = json::str(post, "preview_file_url"); !preview.empty()) {
			item.poster = Image{ .url = std::move(preview) };
		}
	}

	// cdn.donmai.us serves the file to a bare header-less GET (verified live), so
	// no Referer is needed — item.image.headers stays empty.
	return item;
}

ImageContainerInfo pool_to_container_info(const boost::json::object& pool) {
	ImageContainerInfo info;
	info.id = static_cast<ImageContainerID>(json::integer(pool, "id"));
	// A pool has a name of its own (unlike a post), so no title synthesis needed.
	info.title = json::str(pool, "name");
	info.description.text = json::str(pool, "description");
	// The pool states its size up front; items() pages through that many posts.
	info.total_items = static_cast<long>(json::integer(pool, "post_count"));
	info.revision = json::str(pool, "updated_at");
	return info;
}

} // namespace aniparse::parsers::danbooru
