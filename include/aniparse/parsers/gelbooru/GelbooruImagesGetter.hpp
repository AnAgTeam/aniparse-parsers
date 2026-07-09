/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/images/Image.hpp"

namespace aniparse::parsers {

/**
 * @brief Root images getter for Gelbooru: search by tags, autocomplete, and URL
 * routing over the DAPI post index (index.php?page=dapi&s=post). Stateless.
 *
 * The free-text query is the raw tag string; sorts map onto `sort:` metatags and
 * exclusions onto `-tag`. The structured DAPI is credential-walled — requests carry
 * the api_key+user_id stamped into the config by GelbooruParser::authenticate_context
 * — while autocomplete (page=autocomplete2) needs no credentials.
 */
class GelbooruImagesGetter : public ImagesGetter {
public:
	NetworkRequestTask<SearchCompatibilities> search_support(RequestorContext context) override;

	NetworkRequestTask<std::vector<SearchSuggestion>> suggest(
	    RequestorContext context,
	    std::string partial,
	    std::optional<std::string> kind) override;

	NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> search(
	    RequestorContext context,
	    SearchRequestQuery query,
	    GetFilters filters) override;

	NetworkRequestTask<PageResults<std::unique_ptr<ImageContainerGetter>>> latest(
	    RequestorContext context,
	    GetFilters filters) override;

	NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> parse_url(
	    RequestorContext context,
	    ParsedUrl url) override;

	NetworkRequestTask<std::unique_ptr<ImageContainerGetter>> from_serialized(
	    SerializedGetterData data) override;
};

} // namespace aniparse::parsers
