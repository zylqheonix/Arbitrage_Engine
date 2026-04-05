#include "binance_ws.hpp"

namespace binance {

WssStream connect(asio::io_context& ioc,
                  ssl::context& ctx,
                  std::string const& host,
                  std::string const& port,
                  std::string const& target) {
  return connect_wss(ioc, ctx, host, port, target);
}

void subscribe_book_ticker(WssStream& ws, std::string const& symbol) {
  json::object msg;
  msg["method"] = "SUBSCRIBE";
  msg["params"] = json::array({symbol + std::string{"@bookTicker"}});
  msg["id"] = 1;
  std::string const payload = json::serialize(msg);
  ws.write(asio::buffer(payload));
}

void read_next(WssStream& ws, QuoteCallback on_quote, ErrorCallback on_error) {
  auto buffer = std::make_shared<beast::flat_buffer>();

  ws.async_read(
      *buffer,
      [buffer, &ws, on_quote, on_error](beast::error_code ec, std::size_t) {
        if (ec) {
          if (ec == boost::asio::error::operation_aborted) return;
          std::cerr << "[binance] read error: " << ec.message() << "\n";
          if (on_error) on_error(ec);
          return;
        }

        try {
          json::object const result =
              json::parse(beast::buffers_to_string(buffer->data()))
                  .as_object();
          buffer->consume(buffer->size());

          if (!result.contains("s") || !result.contains("b") ||
              !result.contains("a")) {
            read_next(ws, on_quote, on_error);
            return;
          }

          QuoteUpdate quote{};
          quote.exchange = Exchange::BINANCE;
          quote.symbol = std::string(result.at("s").as_string());
          quote.best_ask =
              std::stod(std::string(result.at("a").as_string()));
          quote.best_bid =
              std::stod(std::string(result.at("b").as_string()));
          quote.bid_qty =
              std::stod(std::string(result.at("B").as_string()));
          quote.ask_qty =
              std::stod(std::string(result.at("A").as_string()));

          auto const now_sys = std::chrono::system_clock::now();
          quote.timestamp_ms =
              std::chrono::duration_cast<std::chrono::milliseconds>(
                  now_sys.time_since_epoch())
                  .count();
          quote.local_time = std::chrono::steady_clock::now();

          on_quote(quote);
        } catch (std::exception const& e) {
          std::cerr << "[binance] parse error: " << e.what() << "\n";
        }

        read_next(ws, on_quote, on_error);
      });
}

}  // namespace binance
