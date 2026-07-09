/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "CoroTest.hpp"

#include "aniparse/parsers/danbooru/DanbooruImagesGetter.hpp"
#include "aniparse/ClientContext.hpp"
#include "aniparse/CookieJar.hpp"

#include <fstream>
#include <memory>
#include <sstream>
#include <string>

// Whole-method tests for the Danbooru search path, driven through a canned client
// so the REAL getter code runs offline: search() -> list_request -> request_json ->
// build_post_page -> container mapping. This is the tier that exercises the request
// building and the page-loop wiring (including a banned post mid-page), none of which
// the pure per-post mapping tests reach — those functions live in an anonymous
// namespace and are only callable through the public method. Runs under ASan.

using namespace aniparse;
using aniparse::parsers::DanbooruImagesGetter;

namespace {

std::string load_body(std::string_view name) {
	std::string path = std::string(ANIPARSE_PARSERS_TEST_FIXTURES_DIR) + "/danbooru/" + std::string(name);
	std::ifstream in(path, std::ios::binary);
	INFO("fixture: " << path);
	REQUIRE(in.good());
	std::ostringstream buffer;
	buffer << in.rdbuf();
	return buffer.str();
}

struct DummyCookieJar : CookieJar {
	std::optional<Cookie> find_cookie(std::string_view) const override { return std::nullopt; }
	std::vector<Cookie> cookies() const override { return {}; }
	void set_cookie(const Cookie&) override {}
	void clear() override {}
	std::vector<std::string> serialize() const override { return {}; }
	void deserialize(std::span<std::string>) override {}
};

struct DummyLogger : LoggerContext {
	void log(LogLevel, std::string_view, std::source_location) override {}
};

// Returns a preset response for every request and records the last one it received,
// so a test can assert both the mapped result and the request the getter built.
struct CannedClientMock : ClientContext {
	explicit CannedClientMock(Response<ResponseData> response)
		: response(std::move(response)) {}

	NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest request) override {
		this->request = std::move(request);
		co_return response;
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest request) override {
		this->request = std::move(request);
		co_return response;
	}
	NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest request) override {
		this->request = std::move(request);
		co_return response;
	}
	void set_config(ClientConfig) override {}
	std::shared_ptr<CookieJar> make_cookie_jar() override { return std::make_shared<DummyCookieJar>(); }

	std::variant<std::monostate,
	             ConfiguredGetRequest,
	             ConfiguredPostRequest,
	             ConfiguredPostMultipartRequest> request;
	Response<ResponseData> response;
};

// A context whose client replays `body` with a 200; the returned mock handle lets
// the caller inspect the request the getter built.
std::shared_ptr<CannedClientMock> make_mock(std::string body) {
	return std::make_shared<CannedClientMock>(
	    ResponseData{ .status_code = 200, .body = std::move(body) });
}

RequestorContext context_over(std::shared_ptr<CannedClientMock> mock) {
	return RequestorContext(std::move(mock),
	                        std::make_shared<DummyLogger>(),
	                        std::make_shared<ParserConfig>());
}

} // namespace

CORO_TEST_CASE("search maps a posts page into container getters") {
	auto mock = make_mock(load_body("posts_page.json"));
	RequestorContext context = context_over(mock);

	DanbooruImagesGetter getter;
	auto page = co_await getter.search(context, SearchRequestQuery{ .query = "cirno" }, GetFilters{});
	REQUIRE(page.has_value());

	// The page carries all three posts (still, video, banned) in order, each with
	// its absolute offset, and reports the next offset past them.
	REQUIRE(page->results.size() == 3);
	CHECK(page->results[0].offset == 0);
	CHECK(page->results[2].offset == 2);
	CHECK(page->next_offset == 3);

	// Post 0 (still): info + a single still leaf, no refetch (cached at build time).
	auto info0 = co_await page->results[0].item->info(context);
	REQUIRE(info0.has_value());
	CHECK(info0->id == 5000010);
	CHECK(info0->title == "cirno (touhou_project)");
	CHECK(info0->tags.size() == 9); // 1 artist + 1 copyright + 1 character + 1 meta + 5 general
	auto items0 = co_await page->results[0].item->items(context, GetFilters{});
	REQUIRE(items0.has_value());
	REQUIRE(items0->results.size() == 1);
	CHECK(items0->results[0].item.kind == ImageItemKind::Still);

	// Post 1 (webm): a single video leaf with a poster.
	auto items1 = co_await page->results[1].item->items(context, GetFilters{});
	REQUIRE(items1.has_value());
	REQUIRE(items1->results.size() == 1);
	CHECK(items1->results[0].item.kind == ImageItemKind::Video);
	CHECK(items1->results[0].item.poster.has_value());

	// Post 2 (banned): metadata survives, but the null file_url yields no media leaf
	// — the discriminating case that only shows up when the page loop runs for real.
	auto info2 = co_await page->results[2].item->info(context);
	REQUIRE(info2.has_value());
	CHECK(info2->id == 5000012);
	CHECK(info2->title == "cirno (original)");
	auto items2 = co_await page->results[2].item->items(context, GetFilters{});
	REQUIRE(items2.has_value());
	CHECK(items2->results.empty());
}

CORO_TEST_CASE("search builds the /posts.json request with tags, page and limit") {
	auto mock = make_mock(load_body("posts_page.json"));
	RequestorContext context = context_over(mock);

	DanbooruImagesGetter getter;
	auto page = co_await getter.search(context, SearchRequestQuery{ .query = "cirno" }, GetFilters{});
	REQUIRE(page.has_value());

	const auto* configured = std::get_if<ConfiguredGetRequest>(&mock->request);
	REQUIRE(configured != nullptr);
	const GetRequest& req = configured->request;

	CHECK(req.url.ends_with("/posts.json"));
	CHECK(req.url_params.get("tags") == "cirno");
	CHECK(req.url_params.get("page") == "1");   // from 0, default limit 20 -> page 1
	CHECK(req.url_params.get("limit") == "20");
}

CORO_TEST_CASE("search folds a supported sort into an order: metatag") {
	auto mock = make_mock(load_body("posts_page.json"));
	RequestorContext context = context_over(mock);

	GetFilters filters;
	filters.sort = SortOrder{ .key = std::string(sort_keys::popularity), .ascending = false };

	DanbooruImagesGetter getter;
	auto page = co_await getter.search(context, SearchRequestQuery{ .query = "cirno" }, filters);
	REQUIRE(page.has_value());

	const auto* configured = std::get_if<ConfiguredGetRequest>(&mock->request);
	REQUIRE(configured != nullptr);
	// The sort rides inside the tag string as an order: metatag (exempt from the
	// anonymous 2-tag limit), not as a separate parameter.
	CHECK(configured->request.url_params.get("tags") == "cirno order:score");
}
