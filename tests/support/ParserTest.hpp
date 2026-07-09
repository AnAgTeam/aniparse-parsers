/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "catch_amalgamated.hpp"

#include "aniparse/ClientContext.hpp"
#include "aniparse/CookieJar.hpp"

#include <boost/json.hpp>

#include <fstream>
#include <memory>
#include <source_location>
#include <sstream>
#include <string>
#include <utility>
#include <variant>

/**
 * @file
 * Shared harness for the parser tests: the inert service doubles every parser test
 * needs (a no-op cookie jar and logger), a client that replays a canned response
 * and records the request it was handed, and helpers to load a fixture relative to
 * the tests directory. Kept parser-agnostic so each parser's tests only bring their
 * own fixtures and assertions.
 */
namespace aniparse::parsertest {

/// A cookie jar that stores nothing — parser tests drive the mapping, not cookies.
struct DummyCookieJar : CookieJar {
	std::optional<Cookie> find_cookie(std::string_view) const override { return std::nullopt; }
	std::vector<Cookie> cookies() const override { return {}; }
	void set_cookie(const Cookie&) override {}
	void clear() override {}
	std::vector<std::string> serialize() const override { return {}; }
	void deserialize(std::span<std::string>) override {}
};

/// A logger that drops every message.
struct DummyLogger : LoggerContext {
	void log(LogLevel, std::string_view, std::source_location) override {}
};

/// A client that returns a preset response for every request and records the last
/// one it received, so a test can assert both the mapped result and the request the
/// getter built (its URL, params and cookies).
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

/// A client mock that replays @p body with @p status for every request.
inline std::shared_ptr<CannedClientMock> make_mock(std::string body, long status = 200) {
	return std::make_shared<CannedClientMock>(
	    ResponseData{ .status_code = status, .body = std::move(body) });
}

/// A context over @p mock with the inert logger and an empty config. The mock is
/// kept alive by the returned context; hold the same handle to inspect the request.
inline RequestorContext context_over(std::shared_ptr<CannedClientMock> mock) {
	return RequestorContext(std::move(mock),
	                        std::make_shared<DummyLogger>(),
	                        std::make_shared<ParserConfig>());
}

/// Read a fixture file by its path relative to the tests directory (e.g.
/// "danbooru/fixtures/post_still.json").
inline std::string read_fixture(std::string_view relative) {
	std::string path = std::string(ANIPARSE_PARSERS_TESTS_DIR) + "/" + std::string(relative);
	std::ifstream in(path, std::ios::binary);
	INFO("fixture: " << path);
	REQUIRE(in.good());
	std::ostringstream buffer;
	buffer << in.rdbuf();
	return buffer.str();
}

/// Read a fixture and parse it as JSON. Bind the result to a named variable before
/// navigating: the returned value owns the tree that as_object()/if_object() borrow.
inline boost::json::value load_fixture_json(std::string_view relative) {
	return boost::json::parse(read_fixture(relative));
}

} // namespace aniparse::parsertest
