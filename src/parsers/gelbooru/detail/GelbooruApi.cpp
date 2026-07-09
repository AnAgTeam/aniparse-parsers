/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/gelbooru/detail/GelbooruApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/ClientContext.hpp"

#include <boost/json.hpp>

#include <array>

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
} // namespace

std::span<const std::string_view> api_hosts() {
	using namespace std::string_view_literals;
	static constexpr std::array hosts = { "https://gelbooru.com"sv };
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

Headers media_headers() {
	// Bare GET on img*.gelbooru.com 302s to a hotlink page; the Referer returns the
	// real bytes (verified live).
	return Headers{
	    { "Referer", "https://gelbooru.com/" },
	};
}

std::optional<ImageContainerID> extract_id(std::string_view query) {
	constexpr std::string_view marker = "id=";
	std::size_t pos = query.find(marker);
	while (pos != std::string_view::npos) {
		// Require a boundary before "id=" so "parent_id=" / "pool_id=" do not match.
		if (pos == 0 || query[pos - 1] == '&' || query[pos - 1] == '?') {
			std::size_t value = pos + marker.size();
			std::size_t end = value;
			while (end < query.size() && query[end] >= '0' && query[end] <= '9') {
				++end;
			}
			if (end > value) {
				return static_cast<ImageContainerID>(std::atol(std::string(query.substr(value, end - value)).c_str()));
			}
		}
		pos = query.find(marker, pos + marker.size());
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

ImageContainerInfo post_to_container_info(const boost::json::object& post) {
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
		info.previews.push_back(Image{ .url = std::move(preview), .headers = media_headers() });
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

std::optional<ImageItem> post_to_item(const boost::json::object& post) {
	std::string file_url = json::str(post, "file_url");
	if (file_url.empty()) {
		return std::nullopt;
	}

	ImageItem item;
	item.kind = derive_kind(file_ext_of(file_url));
	item.image.url = std::move(file_url);
	// Every media fetch needs the Referer (hotlink protection).
	item.image.headers = media_headers();

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
			item.poster = Image{ .url = std::move(poster), .headers = media_headers() };
		}
	}

	return item;
}

} // namespace aniparse::parsers::gelbooru
