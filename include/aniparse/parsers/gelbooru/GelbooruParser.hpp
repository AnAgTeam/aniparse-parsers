/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/Parser.hpp"

namespace aniparse::parsers {

/**
 * @brief Gelbooru (gelbooru.com) — a public, tag-indexed image board.
 *
 * A second images-domain showcase, and the first credentialed one: unlike Danbooru,
 * Gelbooru's structured DAPI is auth-walled — every read needs a user-supplied
 * api_key + user_id (query params). authenticate_context stamps them into the
 * config; without them the DAPI answers 401. Media fetches additionally need a
 * Referer (hotlink protection). Autocomplete is the one credential-free surface.
 */
class GelbooruParser : public Parser {
public:
	std::string name() const override;
	std::string identifier() const override;
	GetterSuggestionType suggest_getter(const ParsedUrl& url) const override;
	ParserCompatibilities compatibilities() const override;
	void emplace_domains(EmplaceDomainsContext& context) const override;
	void configure(ParserConfig& config) const override;
	std::span<const std::string_view> mirrors() const override;
	std::unique_ptr<ImagesGetter> images_getter() const override;

	/// Fold a user's Gelbooru credentials into the config: AuthenticationUserPassword
	/// carries user_id as @c username and api_key as @c password, stamped as query
	/// params. No network round-trip — Gelbooru credentials are static query params.
	NetworkRequestTask<std::shared_ptr<const ParserConfig>> authenticate_context(
	    RequestorContext context,
	    AuthenticationData data) override;

	/// The api_key + user_id url params are the durable credential to persist.
	AuthKeys auth_keys() const noexcept override;
};

} // namespace aniparse::parsers
