/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "catch_amalgamated.hpp"

#include "aniparse/ClientContext.hpp"
#include "aniparse/net/CookieJar.hpp"
#include "aniparse/Parser.hpp"

#include <boost/json.hpp>

#include <cstdlib>
#include <fstream>
#include <functional>
#include <memory>
#include <source_location>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
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

// --- Fixture vs live data source ------------------------------------------------
//
// A behaviour test is written once and run against either the canned fixture
// (default, deterministic, what CI runs) or the real API — chosen at runtime by the
// ANIPARSE_TEST_LIVE environment variable, not by a second copy of the test. The
// live transport is INJECTED (set_live_client_factory), so nothing here is tied to a
// particular HTTP backend; a single registrar TU installs whatever client the build
// has, and a build with none simply has no live mode.

/// True when ANIPARSE_TEST_LIVE is set to a non-empty, non-"0" value. Read once.
inline bool live_mode() {
	static const bool live = [] {
		const char* value = std::getenv("ANIPARSE_TEST_LIVE");
		return value && *value && std::string_view(value) != "0";
	}();
	return live;
}

using LiveClientFactory = std::function<std::shared_ptr<ClientContext>()>;

/// The process-wide factory for the real HTTP client, installed by a registrar TU
/// (@see set_live_client_factory). Empty when the build ships no live transport.
inline LiveClientFactory& live_client_factory() {
	static LiveClientFactory factory;
	return factory;
}

/// Install the real-client factory. Backend-agnostic: the caller decides which
/// concrete ClientContext to build (curl, NSURLSession, ...), keeping the tests and
/// this header free of any backend dependency.
inline void set_live_client_factory(LiveClientFactory factory) {
	live_client_factory() = std::move(factory);
}

/// A context whose data is either the canned @p fixture_rel (default) or the live
/// API for @p parser (ANIPARSE_TEST_LIVE set). In live mode the parser's make_config
/// stamps its id + headers onto the real transport, exactly as production would.
/// @throws std::runtime_error in live mode when no live client was registered.
inline RequestorContext data_context(const Parser& parser, std::string_view fixture_rel) {
	if (!live_mode()) {
		return context_over(make_mock(read_fixture(fixture_rel)));
	}
	const LiveClientFactory& factory = live_client_factory();
	if (!factory) {
		throw std::runtime_error("ANIPARSE_TEST_LIVE is set but no live client is registered in this build");
	}
	RequestorContext base(factory(), std::make_shared<DummyLogger>(), nullptr);
	return base.new_with_config(parser.make_config(base.config()));
}

} // namespace aniparse::parsertest
