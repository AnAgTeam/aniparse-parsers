/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/anilist/detail/AniListApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/UrlPath.hpp"

#include <boost/json.hpp>

#include <array>

namespace aniparse::parsers::anilist {

namespace {
	/// AniList descriptions embed literal <br> line breaks (even with
	/// asHtml:false); normalize them to '\n' so the plain text reads cleanly.
	std::string strip_html_breaks(std::string text) {
		for (std::string_view tag : { "<br>", "<br/>", "<br />" }) {
			std::size_t pos = 0;
			while ((pos = text.find(tag, pos)) != std::string::npos) {
				text.replace(pos, tag.size(), "\n");
				pos += 1;
			}
		}
		return text;
	}

	/// Map an AniList MediaStatus to the model's canonical aired-status name.
	/// RELEASING/HIATUS read as ongoing; NOT_YET_RELEASED as announced; FINISHED
	/// as released; CANCELLED (and anything unknown) falls through to Other.
	AiredStatus map_status(std::string_view status) {
		if (status == "FINISHED") {
			return AiredStatus{ .name = std::string(aired_status_released) };
		}
		if (status == "RELEASING" || status == "HIATUS") {
			return AiredStatus{ .name = std::string(aired_status_ongoing) };
		}
		if (status == "NOT_YET_RELEASED") {
			return AiredStatus{ .name = std::string(aired_status_announced) };
		}
		return AiredStatus{};
	}

	/// native title as original_title when it is present and differs from the
	/// chosen display title (so we don't echo the same string twice).
	void set_original_title(MangaInfo& info, const boost::json::object& media) {
		const boost::json::object* title = aniparse::json::object_field(media, "title");
		if (!title) {
			return;
		}
		if (std::string native = aniparse::json::str(*title, "native");
		    !native.empty() && native != info.title) {
			info.original_title = std::move(native);
		}
	}
} // namespace

std::span<const std::string_view> api_hosts() {
	using namespace std::string_view_literals;
	// One official endpoint today; a live catalog override can supersede it.
	static constexpr std::array hosts = {
	    "https://graphql.anilist.co"sv,
	};
	return hosts;
}

std::string_view get_api_base(const RequestorContext& context) {
	return context.base_url(api_hosts());
}

Headers api_headers() {
	return Headers{
	    { "Content-Type", "application/json" },
	    { "Accept",       "application/json" },
	    { "User-Agent",   "Mozilla/5.0 (Linux; Android 13) AppleWebKit/537.36 "
	                      "(KHTML, like Gecko) Chrome/119.0.0.0 Mobile Safari/537.36" },
	};
}

std::string graphql_body(std::string_view query, boost::json::object variables) {
	boost::json::object body;
	body["query"]     = boost::json::string(query);
	body["variables"] = std::move(variables);
	return boost::json::serialize(body);
}

const boost::json::object* graphql_data(const boost::json::value& envelope) {
	return aniparse::json::object_field(envelope.if_object(), "data");
}

std::string graphql_error(const boost::json::value& envelope) {
	const boost::json::object* root = envelope.if_object();
	if (!root) {
		return {};
	}
	const boost::json::array* errors = aniparse::json::array_field(root, "errors");
	if (!errors || errors->empty()) {
		return {};
	}
	const boost::json::object* first = errors->front().if_object();
	return first ? aniparse::json::str(*first, "message") : std::string{};
}

std::optional<int> extract_media_id(std::string_view path) {
	if (auto id = numeric_after(path, "/manga/")) {
		return static_cast<int>(*id);
	}
	return std::nullopt;
}

std::string display_title(const boost::json::object& media) {
	const boost::json::object* title = aniparse::json::object_field(media, "title");
	if (!title) {
		return {};
	}
	if (std::string english = aniparse::json::str(*title, "english"); !english.empty()) {
		return english;
	}
	if (std::string romaji = aniparse::json::str(*title, "romaji"); !romaji.empty()) {
		return romaji;
	}
	return aniparse::json::str(*title, "native");
}

MangaInfo media_to_preview(const boost::json::object& media) {
	MangaInfo info;
	info.id    = static_cast<MangaID>(aniparse::json::integer(media, "id"));
	info.title = display_title(media);
	set_original_title(info, media);
	if (const boost::json::object* cover = aniparse::json::object_field(media, "coverImage")) {
		std::string url = aniparse::json::str(*cover, "large");
		if (url.empty()) {
			url = aniparse::json::str(*cover, "extraLarge");
		}
		if (!url.empty()) {
			info.previews.push_back(Image{ .url = std::move(url) });
		}
	}
	return info;
}

MangaInfo media_to_info(const boost::json::object& media) {
	namespace json = aniparse::json;

	MangaInfo info;
	info.id    = static_cast<MangaID>(json::integer(media, "id"));
	info.title = display_title(media);
	set_original_title(info, media);

	if (std::string description = json::str(media, "description"); !description.empty()) {
		info.description = AttributedText{ .text = strip_html_breaks(std::move(description)) };
	}

	// coverImage.extraLarge, falling back to large.
	if (const boost::json::object* cover = json::object_field(media, "coverImage")) {
		std::string url = json::str(*cover, "extraLarge");
		if (url.empty()) {
			url = json::str(*cover, "large");
		}
		if (!url.empty()) {
			info.previews.push_back(Image{ .url = std::move(url) });
		}
	}

	// genres (array of plain strings) and tags ({name}) both fold into tags.
	if (const boost::json::array* genres = json::array_field(media, "genres")) {
		for (const boost::json::value& genre : *genres) {
			if (const boost::json::string* name = genre.if_string(); name && !name->empty()) {
				info.tags.push_back(Tag{ .name = std::string(name->c_str(), name->size()) });
			}
		}
	}
	if (const boost::json::array* tags = json::array_field(media, "tags")) {
		for (const boost::json::value& entry : *tags) {
			const boost::json::object* tag = entry.if_object();
			if (!tag) {
				continue;
			}
			if (std::string name = json::str(*tag, "name"); !name.empty()) {
				info.tags.push_back(Tag{ .name = std::move(name) });
			}
		}
	}

	// staff edges: role "Story" -> author, "Art" -> artist (a "Story & Art"
	// credit hits both). First credit of each kind wins.
	if (const boost::json::object* staff = json::object_field(media, "staff")) {
		if (const boost::json::array* edges = json::array_field(*staff, "edges")) {
			for (const boost::json::value& entry : *edges) {
				const boost::json::object* edge = entry.if_object();
				if (!edge) {
					continue;
				}
				std::string name;
				if (const boost::json::object* node = json::object_field(*edge, "node")) {
					name = json::str(json::object_field(*node, "name"), "full");
				}
				if (name.empty()) {
					continue;
				}
				const std::string role = json::str(*edge, "role");
				if (role.find("Story") != std::string::npos && info.author.name.empty()) {
					info.author = RelatedUser{ .name = name };
				}
				if (role.find("Art") != std::string::npos && info.artist.name.empty()) {
					info.artist = RelatedUser{ .name = name };
				}
			}
		}
	}

	// averageScore is a 0-100 mean.
	if (long score = static_cast<long>(json::integer(media, "averageScore")); score > 0) {
		info.rating = Rating::from_score(static_cast<double>(score), 100);
	}

	info.status = map_status(json::str(media, "status"));

	if (long chapters = static_cast<long>(json::integer(media, "chapters")); chapters > 0) {
		info.total_chapters = chapters;
	}

	info.is_hentai = json::boolean(media, "isAdult");
	return info;
}

} // namespace aniparse::parsers::anilist
