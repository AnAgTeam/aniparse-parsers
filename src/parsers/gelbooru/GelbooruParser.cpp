/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/gelbooru/GelbooruParser.hpp"
#include "aniparse/parsers/gelbooru/GelbooruImagesGetter.hpp"
#include "aniparse/parsers/gelbooru/detail/GelbooruApi.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/utility/Coroutines.hpp"

#include <memory>
#include <variant>

namespace aniparse::parsers {

ParserInfo GelbooruParser::info() const {
	return {
	    .name             = "Gelbooru",
	    .primary_language = "en",
	};
}

std::string GelbooruParser::identifier() const {
	return "Gelbooru";
}

GetterSuggestionType GelbooruParser::suggest_getter(const ParsedUrl& url) const {
	// Gelbooru addresses a post at ?page=post&s=view&id=<n> and a pool at ?s=pool;
	// both are image containers. Route anything carrying an id we can read.
	return gelbooru::extract_id(url.query()) ? GetterSuggestionType::Images
	                                         : GetterSuggestionType::Unknown;
}

ParserCompatibilities GelbooruParser::compatibilities() const {
	return {
	    .flags = compatibilities_flags::supports_images_search
	           | compatibilities_flags::adult_source,
	};
}

void GelbooruParser::emplace_domains(EmplaceDomainsContext& context) const {
	context.add_domain("gelbooru.com");
}

void GelbooruParser::configure(ParserConfig& config) const {
	for (const auto& [name, value] : gelbooru::api_headers()) {
		config.headers.set(name, value);
	}
}

std::span<const std::string_view> GelbooruParser::mirrors() const {
	return gelbooru::api_hosts();
}

std::unique_ptr<ImagesGetter> GelbooruParser::images_getter() const {
	return std::make_unique<GelbooruImagesGetter>();
}

NetworkRequestTask<std::shared_ptr<const ParserConfig>> GelbooruParser::authenticate_context(
    RequestorContext context, AuthenticationData data) {
	const auto* creds = std::get_if<AuthenticationUserPassword>(&data);
	if (!creds) {
		co_return make_response_error(RequestErrorCode::InvalidArguments,
		    "Gelbooru credentials must be user_id (username) + api_key (password)");
	}

	// Static query-param credentials: copy the base config and stamp them in. The
	// merged url_params ride on every request (ClientContext applies config params).
	auto config = std::make_shared<ParserConfig>(*context.config());
	config->url_params["user_id"] = creds->username;
	config->url_params["api_key"] = creds->password;
	co_return std::shared_ptr<const ParserConfig>(std::move(config));
}

AuthKeys GelbooruParser::auth_keys() const noexcept {
	AuthKeys keys;
	keys.url_params = { "api_key", "user_id" };
	return keys;
}

} // namespace aniparse::parsers
