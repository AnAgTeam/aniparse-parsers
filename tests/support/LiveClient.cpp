/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "support/ParserTest.hpp"

// The single place that names a concrete HTTP backend. It registers the real client
// used when ANIPARSE_TEST_LIVE is set, so the rest of the test suite (and every
// parser's tests) stay backend-neutral. Swap the body for another transport
// (NSURLSession, ...) without touching a single test. A build with no backend
// compiles this to nothing, and live mode is simply unavailable.
#if defined(ANIPARSE_CURL_BACKEND)

#include <aniparse/testing/FileCachingClient.hpp>

#include <filesystem>
#include <memory>

namespace {

// A disk-caching client over the real network: the first live run populates the
// cache, later runs replay from it — so re-running the live tests does not re-hit
// the source (and its rate limits). The one-arg ctor defaults the upstream to the
// curl AsyncClient.
const bool registered_live_client = [] {
	aniparse::parsertest::set_live_client_factory([] {
		std::filesystem::path cache_dir =
		    std::filesystem::temp_directory_path() / "aniparse-parsers-live-cache";
		return std::make_shared<aniparse::testing::FileCachingClient>(std::move(cache_dir));
	});
	return true;
}();

} // namespace

#endif // ANIPARSE_CURL_BACKEND
