/*
 * Copyright (C) 2025-2026 Toilettrauma
 *
 * Author: Toilettrauma <macosinternal@gmail.com>
 */
#pragma once
#include "aniparse/ClientContext.hpp"

#include <memory>

namespace aniparse::parsers::demo {

/**
 * @brief Wrap a transport so requests for the demo source's own images resolve to
 * its embedded bytes instead of the network.
 *
 * The demo parser emits demo:// URLs for its covers and pages; this decorator
 * intercepts exactly those (serving @ref asset_bytes with a 200), and forwards
 * every other request — every real source's traffic — to @p inner unchanged. The
 * session installs it once around whichever HTTP backend was built, so the demo
 * source reads with no network while nothing else is affected.
 * @param inner The real transport to delegate to. Must be non-null.
 * @return The wrapping client (or @p inner unchanged if it is null).
 */
[[nodiscard]] std::shared_ptr<ClientContext> wrap_demo_assets(std::shared_ptr<ClientContext> inner);

} // namespace aniparse::parsers::demo
