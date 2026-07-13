/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"
#include "support/ParserTest.hpp"

#include "aniparse/parsers/anilist/AniListMangaRootGetter.hpp"

#include <memory>

// AniList's search() is the one getter that issues TWO requests: it fetches the
// genre catalog through search_support() before running the query itself. That made
// it the one place where a cancelled request could be swallowed — search_support
// deliberately degrades on failure (serve sorts, skip genres), and a cancellation
// took that same path: it reported success with an empty filter set, search() then
// fired its own request anyway, and a genre query came back as InvalidArguments
// rather than Cancelled.
//
// Cancellation is not degradation, so it must propagate. Driven through a client
// double that fails every request the way a cancelled transfer does, so this runs
// offline and asserts on the getter's contract, not on timing.

using namespace aniparse;
using aniparse::parsers::AniListMangaRootGetter;

namespace {

/// A client that cancels every request, and counts how many it was asked to make.
struct CancellingClientMock : ClientContext {
	NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest) override {
		++requests;
		co_return cancelled();
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest) override {
		++requests;
		co_return cancelled();
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest) override {
		++requests;
		co_return cancelled();
	}
	void set_config(ClientConfig) override {}
	std::shared_ptr<CookieJar> make_cookie_jar() override {
		return std::make_shared<parsertest::DummyCookieJar>();
	}

	static Response<ResponseData> cancelled() {
		return make_response_error(RequestErrorCode::Cancelled, "cancelled");
	}

	int requests = 0;
};

RequestorContext context_over(std::shared_ptr<CancellingClientMock> mock) {
	return RequestorContext(std::move(mock),
	                        std::make_shared<parsertest::DummyLogger>(),
	                        std::make_shared<ParserConfig>());
}

} // namespace

CORO_TEST_CASE("search reports a cancelled genre fetch as cancelled", "[anilist]") {
	auto mock = std::make_shared<CancellingClientMock>();
	RequestorContext context = context_over(mock);

	AniListMangaRootGetter getter;
	auto page = co_await getter.search(context, SearchRequestQuery{ .query = "frieren" }, GetFilters{});

	REQUIRE_FALSE(page.has_value());
	// Not NetworkError, and above all not InvalidArguments — which is what came back
	// before, because the swallowed cancel left search_support advertising no filters.
	REQUIRE(page.error().code == RequestErrorCode::Cancelled);

	// And it stopped at the first request: a cancelled search must not put a second
	// one on the wire.
	REQUIRE(mock->requests == 1);
}

CORO_TEST_CASE("search_support reports a cancelled request rather than degrading", "[anilist]") {
	auto mock = std::make_shared<CancellingClientMock>();
	RequestorContext context = context_over(mock);

	AniListMangaRootGetter getter;
	auto support = co_await getter.search_support(context);

	// A genuine network failure still degrades to "sorts only" (that is deliberate),
	// but a cancellation is the caller asking to stop, so it must surface.
	REQUIRE_FALSE(support.has_value());
	REQUIRE(support.error().code == RequestErrorCode::Cancelled);
}
