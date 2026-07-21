/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#include "aniparse/parsers/demo/DemoAssetClient.hpp"
#include "aniparse/parsers/demo/DemoAssets.hpp"
#include "aniparse/parsers/demo/DemoData.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace aniparse::parsers::demo {
namespace {

/// A coroutine that answers one demo:// request from the embedded bytes. Only
/// demo URLs reach it; real traffic never enters a coroutine here (see below).
NetworkRequestTask<ResponseData> serve_asset(std::string url) {
	const std::string_view key = std::string_view(url).substr(kScheme.size());
	if (auto bytes = asset_bytes(key)) {
		ResponseData response;
		response.status_code = 200;
		response.body.assign(reinterpret_cast<const char*>(bytes->data()), bytes->size());
		co_return response;
	}
	co_return make_response_error(RequestErrorCode::NotFound,
	                              "demo asset not found: " + std::string(key));
}

/**
 * Delegates to a real transport, intercepting only the demo source's own image
 * URLs. Real requests are tail-returned to @c inner_ with no coroutine frame added,
 * so their cancellation and timing are exactly as if this decorator were absent.
 */
class DemoAssetClient final : public ClientContext {
public:
	explicit DemoAssetClient(std::shared_ptr<ClientContext> inner) : inner_(std::move(inner)) {}

	NetworkRequestTask<ResponseData> do_request(ConfiguredGetRequest request) override {
		if (std::string_view(request.request.url).starts_with(kScheme)) {
			return serve_asset(request.request.url);
		}
		return inner_->do_request(std::move(request));
	}

	NetworkRequestTask<ResponseData> do_request(ConfiguredPostRequest request) override {
		return inner_->do_request(std::move(request));
	}

	NetworkRequestTask<ResponseData> do_request(ConfiguredPostMultipartRequest request) override {
		return inner_->do_request(std::move(request));
	}

	void set_config(ClientConfig config) override { inner_->set_config(std::move(config)); }

	std::shared_ptr<CookieJar> make_cookie_jar() override { return inner_->make_cookie_jar(); }

private:
	std::shared_ptr<ClientContext> inner_;
};

} // namespace

std::shared_ptr<ClientContext> wrap_demo_assets(std::shared_ptr<ClientContext> inner) {
	if (!inner) {
		return inner;
	}
	return std::make_shared<DemoAssetClient>(std::move(inner));
}

} // namespace aniparse::parsers::demo
