#include "config.hpp"
#include "csv_logger.hpp"
#include "engine/engine.hpp"
#include "exchanges/binance_ws.hpp"
#include "exchanges/coinbase_ws.hpp"

#include <boost/asio/signal_set.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/steady_timer.hpp>

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>

namespace asio = boost::asio;
namespace ssl  = asio::ssl;
using beast::error_code;

// ── Forward declarations for mutual recursion ───────────────────────────
static void start_coinbase(asio::io_context& ioc, ssl::context& tls,
                           Config const& cfg, Engine& engine,
                           std::shared_ptr<asio::steady_timer> timer,
                           std::shared_ptr<std::optional<WssStream>> ws,
                           std::chrono::milliseconds delay);

static void start_binance(asio::io_context& ioc, ssl::context& tls,
                          Config const& cfg, Engine& engine,
                          std::shared_ptr<asio::steady_timer> timer,
                          std::shared_ptr<std::optional<WssStream>> ws,
                          std::size_t ep_idx,
                          std::chrono::milliseconds delay);

// ── Shared tick handler ─────────────────────────────────────────────────
static void on_tick(char const* tag, QuoteUpdate const& q, Engine& engine,
                    double threshold) {
  std::cerr << std::fixed << std::setprecision(2);
  std::cerr << tag << " " << q.symbol << " bid=" << q.best_bid
            << " ask=" << q.best_ask << " bid_sz=" << q.bid_qty
            << " ask_sz=" << q.ask_qty << "\n";

  EvalResult const r = engine.evaluate(q);

  if (r.best_net > threshold) {
    std::cerr << std::fixed << std::setprecision(4);
    std::cerr << "[arb] buy_bn_sell_cb=" << r.net_buy_bn_sell_cb
              << " buy_cb_sell_bn=" << r.net_buy_cb_sell_bn
              << " best=" << r.best_net
              << " fresh=" << (r.fresh ? "yes" : "no")
              << " cb_age=" << r.cb_age_ms << "ms"
              << " bn_age=" << r.bn_age_ms << "ms"
              << " samples=" << engine.spread_log_size() << "\n";
  }
}

// ── Backoff helper ──────────────────────────────────────────────────────
static std::chrono::milliseconds next_delay(std::chrono::milliseconds cur,
                                            ReconnectConfig const& rc) {
  auto next = std::chrono::milliseconds(
      static_cast<long long>(cur.count() * rc.backoff_multiplier));
  return std::min(next, rc.max_delay);
}

// ── Coinbase feed lifecycle ─────────────────────────────────────────────
static void start_coinbase(asio::io_context& ioc, ssl::context& tls,
                           Config const& cfg, Engine& engine,
                           std::shared_ptr<asio::steady_timer> timer,
                           std::shared_ptr<std::optional<WssStream>> ws,
                           std::chrono::milliseconds delay) {
  auto const& ep = cfg.coinbase.endpoint;
  try {
    ws->emplace(coinbase::connect(ioc, tls, ep.host, ep.port, "/"));
    coinbase::subscribe_ticker(**ws, cfg.coinbase.symbol);
    std::cerr << "[coinbase] connected (" << ep.host << ":" << ep.port
              << ").\n";

    double const thr = cfg.engine.min_edge_threshold;
    coinbase::read_next(
        **ws,
        [&engine, thr](QuoteUpdate const& q) {
          on_tick("[coinbase]", q, engine, thr);
        },
        [&ioc, &tls, &cfg, &engine, timer, ws](error_code) {
          ws->reset();
          auto d = cfg.reconnect.initial_delay;
          std::cerr << "[coinbase] disconnected, reconnecting in "
                    << d.count() << " ms …\n";
          timer->expires_after(d);
          timer->async_wait([&ioc, &tls, &cfg, &engine, timer, ws,
                             d](error_code ec) {
            if (ec) return;
            start_coinbase(ioc, tls, cfg, engine, timer, ws,
                           next_delay(d, cfg.reconnect));
          });
        });
  } catch (std::exception const& e) {
    ws->reset();
    std::cerr << "[coinbase] connect failed: " << e.what()
              << " — retry in " << delay.count() << " ms\n";
    timer->expires_after(delay);
    timer->async_wait(
        [&ioc, &tls, &cfg, &engine, timer, ws, delay](error_code ec) {
          if (ec) return;
          start_coinbase(ioc, tls, cfg, engine, timer, ws,
                         next_delay(delay, cfg.reconnect));
        });
  }
}

// ── Binance feed lifecycle ──────────────────────────────────────────────
static void start_binance(asio::io_context& ioc, ssl::context& tls,
                          Config const& cfg, Engine& engine,
                          std::shared_ptr<asio::steady_timer> timer,
                          std::shared_ptr<std::optional<WssStream>> ws,
                          std::size_t ep_idx,
                          std::chrono::milliseconds delay) {
  auto const& endpoints = cfg.binance.endpoints;
  if (endpoints.empty()) {
    std::cerr << "[binance] no endpoints configured.\n";
    return;
  }
  std::size_t const idx = ep_idx % endpoints.size();
  auto const& ep = endpoints[idx];

  try {
    ws->emplace(binance::connect(ioc, tls, ep.host, ep.port,
                                 cfg.binance.target));
    binance::subscribe_book_ticker(**ws, cfg.binance.symbol);
    std::cerr << "[binance] connected (" << ep.host << ":" << ep.port
              << ").\n";

    double const thr = cfg.engine.min_edge_threshold;
    binance::read_next(
        **ws,
        [&engine, thr](QuoteUpdate const& q) {
          on_tick("[binance]", q, engine, thr);
        },
        [&ioc, &tls, &cfg, &engine, timer, ws, idx](error_code) {
          ws->reset();
          auto d = cfg.reconnect.initial_delay;
          std::cerr << "[binance] disconnected, reconnecting in "
                    << d.count() << " ms …\n";
          timer->expires_after(d);
          timer->async_wait([&ioc, &tls, &cfg, &engine, timer, ws, idx,
                             d](error_code ec) {
            if (ec) return;
            start_binance(ioc, tls, cfg, engine, timer, ws, idx,
                          next_delay(d, cfg.reconnect));
          });
        });
  } catch (std::exception const& e) {
    ws->reset();
    std::cerr << "[binance] " << ep.host << ":" << ep.port
              << " failed: " << e.what() << "\n";
    // Try next endpoint immediately if we haven't cycled through all yet.
    if (idx + 1 < endpoints.size()) {
      start_binance(ioc, tls, cfg, engine, timer, ws, idx + 1, delay);
    } else {
      std::cerr << "[binance] all endpoints exhausted — retry in "
                << delay.count() << " ms\n";
      timer->expires_after(delay);
      timer->async_wait(
          [&ioc, &tls, &cfg, &engine, timer, ws, delay](error_code ec) {
            if (ec) return;
            start_binance(ioc, tls, cfg, engine, timer, ws, 0,
                          next_delay(delay, cfg.reconnect));
          });
    }
  }
}

// ── main ────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
  std::string config_path = "config.json";
  if (argc > 1) config_path = argv[1];

  Config cfg;
  try {
    cfg = load_config(config_path);
  } catch (std::exception const& e) {
    std::cerr << "config error: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  asio::io_context ioc;
  ssl::context tls{ssl::context::tls_client};
  tls.set_default_verify_paths();
  tls.set_verify_mode(ssl::verify_peer);

  CsvLogger csv(cfg.csv);
  Engine engine(cfg.engine, cfg.coinbase.fee_rate, cfg.binance.fee_rate, &csv);

  // Timers for reconnect backoff (one per feed so they're independent).
  auto cb_timer = std::make_shared<asio::steady_timer>(ioc);
  auto bn_timer = std::make_shared<asio::steady_timer>(ioc);

  // Owning pointers: stream lives inside an optional behind a shared_ptr
  // so lambdas in the async chain can reference it safely across reconnects.
  auto cb_ws = std::make_shared<std::optional<WssStream>>();
  auto bn_ws = std::make_shared<std::optional<WssStream>>();

  start_coinbase(ioc, tls, cfg, engine, cb_timer, cb_ws,
                 cfg.reconnect.initial_delay);
  start_binance(ioc, tls, cfg, engine, bn_timer, bn_ws, 0,
                cfg.reconnect.initial_delay);

  // Graceful shutdown on SIGINT / SIGTERM.
  asio::signal_set signals(ioc, SIGINT, SIGTERM);
  signals.async_wait([&](error_code, int sig) {
    std::cerr << "\nCaught signal " << sig << " — shutting down.\n";
    cb_timer->cancel();
    bn_timer->cancel();
    error_code ec;
    if (*cb_ws && (**cb_ws).is_open()) {
      (**cb_ws).close(beast::websocket::close_code::normal, ec);
    }
    if (*bn_ws && (**bn_ws).is_open()) {
      (**bn_ws).close(beast::websocket::close_code::normal, ec);
    }
    csv.flush();
    ioc.stop();
  });

  std::cerr << "Streaming (Ctrl+C to stop). CSV → " << cfg.csv.path << "\n";
  ioc.run();
  csv.flush();
  return EXIT_SUCCESS;
}
