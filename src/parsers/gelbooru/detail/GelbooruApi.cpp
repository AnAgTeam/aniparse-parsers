/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/gelbooru/detail/GelbooruApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/utility/UrlPath.hpp"

#include <boost/json.hpp>

namespace aniparse::parsers::gelbooru {

namespace {
	namespace json = aniparse::json;

	/// The file extension of a url (lowercased tail after the last '.'), ignoring
	/// any query string. Empty when there is none.
	std::string_view file_ext_of(std::string_view url) {
		std::size_t query = url.find('?');
		if (query != std::string_view::npos) {
			url = url.substr(0, query);
		}
		std::size_t dot = url.rfind('.');
		if (dot == std::string_view::npos || dot + 1 >= url.size()) {
			return {};
		}
		return url.substr(dot + 1);
	}

	/// Gelbooru posts carry a single space-separated tag string (no per-category
	/// split). Each token becomes a Tag whose ref is the raw token that round-trips
	/// into a `tags=` search (the token-as-key invariant).
	void append_tags(std::vector<Tag>& tags, const boost::json::object& post) {
		// Own the string: json::str returns by value, so a string_view over it would
		// dangle (Json.hpp's temporary caveat).
		std::string names = json::str(post, "tags");
		std::size_t start = 0;
		while (start < names.size()) {
			std::size_t end = names.find(' ', start);
			if (end == std::string::npos) {
				end = names.size();
			}
			if (end > start) {
				std::string token(names.substr(start, end - start));
				tags.push_back(Tag{ .name = token, .ref = token });
			}
			start = end + 1;
		}
	}

	/// The media fetch headers for a post: a Referer when the site descriptor supplies
	/// one (Gelbooru's img host 302s a bare GET to a hotlink page; the Referer returns
	/// the real bytes), empty otherwise. The host is site data — not baked in here.
	Headers referer_headers(std::optional<std::string_view> media_referer) {
		Headers headers;
		if (media_referer) {
			headers.set("Referer", std::string(*media_referer));
		}
		return headers;
	}
} // namespace

Headers api_headers() {
	return Headers{
	    { "User-Agent", "aniparse-parsers/0.1 (+https://github.com/aniparse)" },
	};
}

std::optional<ImageContainerID> extract_id(std::string_view query) {
	// Require a boundary before "id=" so "parent_id=" / "pool_id=" do not match.
	if (auto id = numeric_after(query, "id=", /*require_boundary=*/true)) {
		return static_cast<ImageContainerID>(*id);
	}
	return std::nullopt;
}

ImageItemKind derive_kind(std::string_view file_ext) {
	if (file_ext == "webm" || file_ext == "mp4") {
		return ImageItemKind::Video;
	}
	if (file_ext == "gif") {
		return ImageItemKind::Animated;
	}
	return ImageItemKind::Still;
}

const boost::json::array* posts_of(const boost::json::value& envelope) {
	// A bare array (some deployments) is already the post list.
	if (const boost::json::array* top = envelope.if_array()) {
		return top;
	}
	// The documented shape wraps the list under "post"; on zero results the key is
	// missing or an empty string, so a non-array value reads as "no posts".
	const boost::json::object* root = envelope.if_object();
	return root ? json::array_field(root, "post") : nullptr;
}

ImageContainerInfo post_to_container_info(const boost::json::object& post,
                                          std::optional<std::string_view> media_referer) {
	ImageContainerInfo info;
	info.id = static_cast<ImageContainerID>(json::integer(post, "id"));
	// A booru post has no title; the flat tag string carries no category to build a
	// readable one from, so synthesize a stable "#<id>".
	info.title = "#" + std::to_string(info.id);

	append_tags(info.tags, post);

	if (std::string owner = json::str(post, "owner"); !owner.empty()) {
		info.uploader = RelatedUser{ .name = owner, .ref = owner };
	}

	if (std::string preview = json::str(post, "preview_url"); !preview.empty()) {
		info.previews.push_back(Image{ .url = std::move(preview), .headers = referer_headers(media_referer) });
	}

	// "change" is a unix timestamp used only as an opaque equality marker.
	if (std::int64_t change = json::integer(post, "change"); change != 0) {
		info.revision = std::to_string(change);
	}

	// Gelbooru rating is g/s/q/e; questionable/explicit are the source's adult tiers.
	std::string rating = json::str(post, "rating");
	info.is_hentai = rating == "explicit" || rating == "questionable" || rating == "e" || rating == "q";
	if (info.is_hentai) {
		info.age_restriction = 18;
	}

	info.total_items = 1;
	return info;
}

std::optional<ImageItem> post_to_item(const boost::json::object& post,
                                      std::optional<std::string_view> media_referer) {
	std::string file_url = json::str(post, "file_url");
	if (file_url.empty()) {
		return std::nullopt;
	}

	ImageItem item;
	item.kind = derive_kind(file_ext_of(file_url));
	item.image.url = std::move(file_url);
	// Media fetches carry the site's Referer (hotlink protection), when it has one.
	item.image.headers = referer_headers(media_referer);

	int width  = static_cast<int>(json::integer(post, "width"));
	int height = static_cast<int>(json::integer(post, "height"));
	if (width > 0 && height > 0) {
		item.image.size = ImageResolution{ .width = width, .height = height };
	}

	// A non-still gets its same-hash preview/sample as a poster.
	if (item.kind != ImageItemKind::Still) {
		std::string poster = json::str(post, "sample_url");
		if (poster.empty()) {
			poster = json::str(post, "preview_url");
		}
		if (!poster.empty()) {
			item.poster = Image{ .url = std::move(poster), .headers = referer_headers(media_referer) };
		}
	}

	return item;
}

} // namespace aniparse::parsers::gelbooru
