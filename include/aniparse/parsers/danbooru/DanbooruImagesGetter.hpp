/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/images/Image.hpp"

namespace aniparse::parsers {

/**
 * @brief Root images getter for Danbooru: search by tags, latest posts, and URL
 * routing over the REST `/posts.json` collection. Stateless — one instance serves
 * every request.
 *
 * The free-text query is the raw tag string (space-separated tags); the requested
 * sort maps onto an `order:` metatag. Anonymous access caps a query at two
 * non-metatags — `rating:`/`order:`/`score:` metatags are exempt — so the public
 * read-path stays within it without credentials.
 */
class DanbooruImagesGetter : public ImagesGetter {
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
