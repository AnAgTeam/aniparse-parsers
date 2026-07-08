/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/kitsu/detail/KitsuApi.hpp"
#include "aniparse/json/Json.hpp"
#include "aniparse/ClientContext.hpp"

#include <boost/json.hpp>

#include <array>
#include <cstdlib>

namespace aniparse::parsers::kitsu {

namespace {
	/// The "attributes" object of a JSON:API resource, nullptr if absent.
	const boost::json::object* attributes_of(const boost::json::object& resource) {
		return aniparse::json::object_field(resource, "attributes");
	}

	/// Display title of a manga: canonicalTitle, else titles.en / en_jp.
	std::string display_title(const boost::json::object& attributes) {
		if (std::string canonical = aniparse::json::str(attributes, "canonicalTitle");
		    !canonical.empty()) {
			return canonical;
		}
		const boost::json::object* titles = aniparse::json::object_field(attributes, "titles");
		if (!titles) {
			return {};
		}
		if (std::string en = aniparse::json::str(*titles, "en"); !en.empty()) {
			return en;
		}
		return aniparse::json::str(*titles, "en_jp");
	}

	/// Map a Kitsu manga status string to the model's canonical aired-status name.
	AiredStatus map_status(std::string_view status) {
		if (status == "finished") {
			return AiredStatus{ .name = std::string(aired_status_released) };
		}
		if (status == "current") {
			return AiredStatus{ .name = std::string(aired_status_ongoing) };
		}
		if (status == "unreleased" || status == "upcoming" || status == "tba") {
			return AiredStatus{ .name = std::string(aired_status_announced) };
		}
		return AiredStatus{};
	}

	/// Numeric MangaID from a JSON:API string id ("38" -> 38), 0 if not numeric.
	MangaID numeric_id(const boost::json::object& resource) {
		std::string id = aniparse::json::str(resource, "id");
		return static_cast<MangaID>(std::atol(id.c_str()));
	}

	/// posterImage.original (else large) -> a single preview image.
	void set_poster(MangaInfo& info, const boost::json::object& attributes) {
		const boost::json::object* poster = aniparse::json::object_field(attributes, "posterImage");
		if (!poster) {
			return;
		}
		std::string url = aniparse::json::str(*poster, "original");
		if (url.empty()) {
			url = aniparse::json::str(*poster, "large");
		}
		if (!url.empty()) {
			info.previews.push_back(Image{ .url = std::move(url) });
		}
	}
} // namespace

std::span<const std::string_view> api_hosts() {
	using namespace std::string_view_literals;
	// kitsu.io primary, kitsu.app fallback (verified live; the brand migrated to
	// kitsu.app). A live catalog override can reorder/replace them.
	static constexpr std::array hosts = {
	    "https://kitsu.io/api/edge"sv,
	    "https://kitsu.app/api/edge"sv,
	};
	return hosts;
}

std::string_view get_api_base(const RequestorContext& context) {
	return context.base_url(api_hosts());
}

Headers api_headers() {
	return Headers{
	    { "Accept",       std::string(media_type) },
	    { "Content-Type", std::string(media_type) },
	    { "User-Agent",   "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
	                      "(KHTML, like Gecko) Chrome/119.0.0.0 Safari/537.36" },
	};
}

const boost::json::object* data_object(const boost::json::value& envelope) {
	return aniparse::json::object_field(envelope.if_object(), "data");
}

const boost::json::array* data_array(const boost::json::value& envelope) {
	return aniparse::json::array_field(envelope.if_object(), "data");
}

std::size_t meta_count(const boost::json::value& envelope) {
	const boost::json::object* meta = aniparse::json::object_field(envelope.if_object(), "meta");
	return static_cast<std::size_t>(aniparse::json::integer(meta, "count"));
}

std::optional<std::string> extract_ref(std::string_view path) {
	constexpr std::string_view marker = "/manga/";
	std::size_t pos = path.find(marker);
	if (pos == std::string_view::npos) {
		return std::nullopt;
	}
	pos += marker.size();
	std::size_t end = pos;
	while (end < path.size() && path[end] != '/' && path[end] != '?') {
		++end;
	}
	if (end == pos) {
		return std::nullopt;
	}
	return std::string(path.substr(pos, end - pos));
}

MangaInfo media_to_preview(const boost::json::object& resource) {
	MangaInfo info;
	info.id = numeric_id(resource);
	const boost::json::object* attributes = attributes_of(resource);
	if (!attributes) {
		return info;
	}
	info.title = display_title(*attributes);
	if (const boost::json::object* titles = aniparse::json::object_field(*attributes, "titles")) {
		if (std::string native = aniparse::json::str(*titles, "ja_jp");
		    !native.empty() && native != info.title) {
			info.original_title = std::move(native);
		}
	}
	set_poster(info, *attributes);
	return info;
}

MangaInfo media_to_info(const boost::json::object& resource, const boost::json::array* included) {
	namespace json = aniparse::json;

	MangaInfo info = media_to_preview(resource);
	const boost::json::object* attributes = attributes_of(resource);
	if (!attributes) {
		return info;
	}

	if (std::string synopsis = json::str(*attributes, "synopsis"); !synopsis.empty()) {
		info.description = AttributedText{ .text = std::move(synopsis) };
	}

	// averageRating is a 0-100 mean, delivered as a string.
	if (std::string rating = json::str(*attributes, "averageRating"); !rating.empty()) {
		if (double value = std::atof(rating.c_str()); value > 0.0) {
			info.rating = Rating::from_score(value, 100);
		}
	}

	info.status = map_status(json::str(*attributes, "status"));

	if (long chapters = static_cast<long>(json::integer(*attributes, "chapterCount")); chapters > 0) {
		info.total_chapters = chapters;
	}

	// Kitsu's coarsest maturity flag: the R18 age rating marks adult content.
	info.is_hentai = json::str(*attributes, "ageRating") == "R18";

	// Categories are sideloaded into the envelope's "included" array; for a
	// single-resource fetch those are exactly this manga's categories.
	if (included) {
		for (const boost::json::value& entry : *included) {
			const boost::json::object* resource_entry = entry.if_object();
			if (!resource_entry || json::str(*resource_entry, "type") != "categories") {
				continue;
			}
			if (const boost::json::object* category_attrs = json::object_field(*resource_entry, "attributes")) {
				if (std::string title = json::str(*category_attrs, "title"); !title.empty()) {
					info.tags.push_back(Tag{ .name = std::move(title) });
				}
			}
		}
	}

	return info;
}

} // namespace aniparse::parsers::kitsu
