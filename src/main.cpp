#include "engine/engine.hpp"
#include "exchanges/binance_ws.hpp"
#include "exchanges/coinbase_ws.hpp"

#include <boost/asio/ssl.hpp>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>

namespace asio = boost::asio;
namespace ssl = asio::ssl;

int main() {
  asio::io_context ioc;

  ssl::context tls{ssl::context::tls_client};
  tls.set_default_verify_paths();
  tls.set_verify_mode(ssl::verify_peer);

  bool any_feed = false;

  Engine engine;

  auto log_feed_and_arb = [&engine](char const* tag, QuoteUpdate const& q) {
    std::cout << std::fixed << std::setprecision(2);
    std::cout << tag << " " << q.symbol << " bid=" << q.best_bid << " ask=" << q.best_ask
              << " bid_sz=" << q.bid_qty << " ask_sz=" << q.ask_qty << "\n";
    double const net = engine.evaluate(q);
    if (net != 0.0) {
      std::cout << std::fixed << std::setprecision(4);
      std::cout << "[arb] net_edge_per_btc=" << net
                << " fresh=" << (engine.is_fresh() ? "yes" : "no")
                << " samples=" << engine.spread_log_size() << "\n";
    }
  };

  // Coinbase first: Binance often returns HTTP 451 (geo-restricted) from some regions.
  std::optional<WssStream> coinbase_ws;
  try {
    coinbase_ws.emplace(coinbase::connect(
        ioc, tls, "ws-feed.exchange.coinbase.com", "443", "/"));
    coinbase::subscribe_ticker(*coinbase_ws, "BTC-USD");
    coinbase::read_next(*coinbase_ws, [&log_feed_and_arb](QuoteUpdate const& q) {
      log_feed_and_arb("[coinbase]", q);
    });
    any_feed = true;
    std::cout << "Coinbase BTC-USD ticker: connected.\n";
  } catch (std::exception const& e) {
    std::cerr << "[coinbase] failed: " << e.what() << "\n";
  }

  // Binance.com may decline the WSS handshake (e.g. HTTP 451) from restricted
  // regions. Try .com on 443/9443, then Binance.US (same stream names / JSON).
  struct {
    char const* host;
    char const* port;
  } constexpr k_binance_endpoints[] = {
      {"stream.binance.com", "443"},
      {"stream.binance.com", "9443"},
      // US-regulated feed (same stream names); 9443 is the documented port.
      {"stream.binance.us", "9443"},
      {"stream.binance.us", "443"},
  };

  std::optional<WssStream> binance_ws;
  for (auto const& ep : k_binance_endpoints) {
    try {
      binance_ws.emplace(
          binance::connect(ioc, tls, ep.host, ep.port, "/ws"));
      binance::subscribe_book_ticker(*binance_ws, "btcusdt");
      binance::read_next(*binance_ws, [&log_feed_and_arb](QuoteUpdate const& q) {
        log_feed_and_arb("[binance]", q);
      });
      any_feed = true;
      std::cout << "Binance btcusdt@bookTicker: connected ("
                << ep.host << ":" << ep.port << ").\n";
      break;
    } catch (std::exception const& e) {
      binance_ws.reset();
      std::cerr << "[binance] " << ep.host << ":" << ep.port
                << " failed: " << e.what() << "\n";
    }
  }
  if (!binance_ws) {
    std::cerr
        << "Hint: If every Binance endpoint failed, your region may block "
           "spot WSS; try a VPN or Binance testnet for development.\n";
  }

  if (!any_feed) {
    std::cerr << "No feeds connected; exiting.\n";
    return EXIT_FAILURE;
  }

  std::cout << "Streaming (Ctrl+C to stop).\n";
  ioc.run();
  return EXIT_SUCCESS;
}
