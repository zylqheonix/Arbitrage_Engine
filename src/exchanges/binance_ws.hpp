#pragma once

#include "exchange_ws_common.hpp"

namespace binance {

WssStream connect(asio::io_context& ioc,
                  ssl::context& ctx,
                  std::string const& host,
                  std::string const& port,
                  std::string const& target);

void subscribe_book_ticker(WssStream& ws, std::string const& symbol);

using QuoteCallback = std::function<void(QuoteUpdate const&)>;
using ErrorCallback = std::function<void(beast::error_code)>;

void read_next(WssStream& ws, QuoteCallback on_quote,
               ErrorCallback on_error = nullptr);

}  // namespace binance
