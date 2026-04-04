#include "coinbase_ws.hpp"

#include <cmath>
#include <limits>
#include <optional>
#include <string_view>

namespace coinbase {

namespace {

bool str_eq(json::value const& v, std::string_view s) {
  return v.is_string() && v.as_string() == s;
}

std::optional<std::string> str_field(json::object const& o,
                                     std::string_view key) {
  auto const it = o.find(key);
  if (it == o.end() || !it->value().is_string()) {
    return std::nullopt;
  }
  return std::string(it->value().as_string());
}

double json_number(json::value const& v) {
  if (v.is_double()) {
    return v.as_double();
  }
  if (v.is_int64()) {
    return static_cast<double>(v.as_int64());
  }
  if (v.is_uint64()) {
    return static_cast<double>(v.as_uint64());
  }
  if (v.is_string()) {
    return std::stod(std::string(v.as_string()));
  }
  return std::numeric_limits<double>::quiet_NaN();
}

double num_field(json::object const& o, std::string_view key, double def = 0.0) {
  auto const it = o.find(key);
  if (it == o.end()) {
    return def;
  }
  try {
    double const x = json_number(it->value());
    return std::isfinite(x) ? x : def;
  } catch (...) {
    return def;
  }
}

}  // namespace

WssStream connect(asio::io_context& ioc,
                  ssl::context& ctx,
                  std::string const& host,
                  std::string const& port,
                  std::string const& target) {
  return connect_wss(ioc, ctx, host, port, target);
}

void subscribe_ticker(WssStream& ws, std::string const& product_id) {
  json::object msg;
  msg["type"] = "subscribe";
  msg["product_ids"] = json::array({product_id});
  msg["channels"] = json::array({"ticker"});
  std::string const payload = json::serialize(msg);
  ws.write(asio::buffer(payload));
}

void read_next(WssStream& ws, std::function<void(QuoteUpdate const&)> on_quote) {
  auto buffer = std::make_shared<beast::flat_buffer>();

  ws.async_read(*buffer, [buffer, &ws, on_quote](beast::error_code ec, std::size_t) {
    if (ec) {
      if (ec == websocket::error::closed) {
        std::cerr << "[coinbase] websocket closed\n";
        return;
      }
      if (ec == boost::asio::error::operation_aborted) {
        std::cerr << "[coinbase] read cancelled\n";
        return;
      }
      std::cerr << "[coinbase] read error: " << ec.message() << "\n";
      return;
    }

    try {
      json::value const jv =
          json::parse(beast::buffers_to_string(buffer->data()));
      buffer->consume(buffer->size());

      json::object const* obj = jv.if_object();
      if (obj != nullptr) {
        json::object const& msg = *obj;
        auto const type_it = msg.find("type");
        if (type_it != msg.end() && str_eq(type_it->value(), "ticker")) {
          auto const product_id = str_field(msg, "product_id");
          if (!product_id) {
            // Partial / non-standard ticker payloads: skip quietly.
          } else {
            QuoteUpdate quote{};
            quote.exchange = Exchange::COINBASE;
            quote.symbol = *product_id;
            quote.best_bid = num_field(msg, "best_bid");
            quote.best_ask = num_field(msg, "best_ask");
            quote.bid_qty = num_field(msg, "best_bid_size");
            quote.ask_qty = num_field(msg, "best_ask_size");

            auto const now_sys = std::chrono::system_clock::now();
            quote.timestamp_ms =
                std::chrono::duration_cast<std::chrono::milliseconds>(
                    now_sys.time_since_epoch())
                    .count();
            quote.local_time = std::chrono::steady_clock::now();

            on_quote(quote);
          }
        }
      }
    } catch (std::exception const& e) {
      std::cerr << "[coinbase] parse error: " << e.what() << "\n";
    }

    read_next(ws, on_quote);
  });
}

}  // namespace coinbase
