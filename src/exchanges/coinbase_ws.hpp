#pragma once

#include "exchange_ws_common.hpp"

namespace coinbase {

WssStream connect(asio::io_context& ioc,
                  ssl::context& ctx,
                  std::string const& host,
                  std::string const& port,
                  std::string const& target);

void subscribe_ticker(WssStream& ws, std::string const& product_id);

void read_next(WssStream& ws, std::function<void(QuoteUpdate const&)> on_quote);

}  // namespace coinbase
