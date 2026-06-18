#include "../src/engine/engine.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

constexpr double kCoinbaseFee = 0.001;
constexpr double kBinanceFee = 0.001;
constexpr double kSlippage = 0.001;

EngineConfig test_config(double min_edge_threshold = 0.0) {
  return EngineConfig{
      .slippage_rate = kSlippage,
      .max_age = std::chrono::milliseconds(1000),
      .min_edge_threshold = min_edge_threshold,
  };
}

QuoteUpdate quote(Exchange exchange, double bid, double ask,
                  std::chrono::steady_clock::time_point local_time =
                      std::chrono::steady_clock::now()) {
  return QuoteUpdate{
      .exchange = exchange,
      .symbol = "BTC-USD",
      .best_ask = ask,
      .best_bid = bid,
      .bid_qty = 1.25,
      .ask_qty = 1.50,
      .timestamp_ms = 123456789,
      .local_time = local_time,
  };
}

void require(bool condition, std::string const& message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << "\n";
    std::exit(EXIT_FAILURE);
  }
}

void require_near(double actual, double expected, double tolerance,
                  std::string const& message) {
  if (std::fabs(actual - expected) > tolerance) {
    std::cerr << "FAIL: " << message << " expected=" << expected
              << " actual=" << actual << "\n";
    std::exit(EXIT_FAILURE);
  }
}

void missing_venue_does_not_compute_spread() {
  Engine engine(test_config(), kCoinbaseFee, kBinanceFee, nullptr);

  EvalResult const result =
      engine.evaluate(quote(Exchange::COINBASE, 100.50, 100.70));

  require(!result.fresh, "single-venue update should not be fresh");
  require_near(result.best_net, 0.0, 1e-12,
               "single-venue update should not compute net spread");
  require(engine.spread_log_size() == 0,
          "single-venue update should not log spreads");
}

void stale_quotes_are_rejected() {
  Engine engine(test_config(), kCoinbaseFee, kBinanceFee, nullptr);
  auto const stale_time =
      std::chrono::steady_clock::now() - std::chrono::milliseconds(1500);

  engine.evaluate(quote(Exchange::COINBASE, 100.50, 100.70, stale_time));
  EvalResult const result =
      engine.evaluate(quote(Exchange::BINANCE, 101.25, 101.50));

  require(!result.fresh, "stale venue pair should not be fresh");
  require_near(result.best_net, 0.0, 1e-12,
               "stale venue pair should not compute net spread");
}

void computes_fee_and_slippage_adjusted_best_edge() {
  Engine engine(test_config(0.10), kCoinbaseFee, kBinanceFee, nullptr);

  engine.evaluate(quote(Exchange::COINBASE, 100.50, 100.70));
  EvalResult const result =
      engine.evaluate(quote(Exchange::BINANCE, 101.25, 101.50));

  double const bn_buy = 101.50 * (1.0 + kSlippage);
  double const cb_sell = 100.50 * (1.0 - kSlippage);
  double const expected_buy_bn_sell_cb =
      (cb_sell - bn_buy) - (kBinanceFee * bn_buy + kCoinbaseFee * cb_sell);

  double const cb_buy = 100.70 * (1.0 + kSlippage);
  double const bn_sell = 101.25 * (1.0 - kSlippage);
  double const expected_buy_cb_sell_bn =
      (bn_sell - cb_buy) - (kCoinbaseFee * cb_buy + kBinanceFee * bn_sell);

  require(result.fresh, "fresh venue pair should be fresh");
  require_near(result.net_buy_bn_sell_cb, expected_buy_bn_sell_cb, 1e-9,
               "buy Binance, sell Coinbase net should include fees/slippage");
  require_near(result.net_buy_cb_sell_bn, expected_buy_cb_sell_bn, 1e-9,
               "buy Coinbase, sell Binance net should include fees/slippage");
  require_near(result.best_net, expected_buy_cb_sell_bn, 1e-9,
               "best net should be the stronger direction");
  require(engine.spread_log_size() == 1,
          "profitable spread above threshold should be logged");
}

void negative_edges_do_not_enter_opportunity_log() {
  Engine engine(test_config(0.10), kCoinbaseFee, kBinanceFee, nullptr);

  engine.evaluate(quote(Exchange::COINBASE, 100.00, 100.10));
  EvalResult const result =
      engine.evaluate(quote(Exchange::BINANCE, 100.00, 100.10));

  require(result.fresh, "equal-market quote pair should be fresh");
  require(result.best_net < 0.0, "fees/slippage should make equal prices negative");
  require(engine.spread_log_size() == 0,
          "negative edges should not be logged as opportunities");
}

void later_quote_replaces_prior_exchange_state() {
  Engine engine(test_config(), kCoinbaseFee, kBinanceFee, nullptr);

  engine.evaluate(quote(Exchange::COINBASE, 100.00, 100.10));
  engine.evaluate(quote(Exchange::BINANCE, 100.00, 100.10));
  engine.evaluate(quote(Exchange::COINBASE, 102.00, 102.10));

  require(engine.coinbase_quote().has_value(),
          "coinbase quote should be retained");
  require_near(engine.coinbase_quote()->best_bid, 102.00, 1e-12,
               "later Coinbase quote should replace prior Coinbase quote");
  require(engine.binance_quote().has_value(), "binance quote should be retained");
  require_near(engine.binance_quote()->best_bid, 100.00, 1e-12,
               "Binance quote should not be replaced by Coinbase updates");
}

}  // namespace

int main() {
  missing_venue_does_not_compute_spread();
  stale_quotes_are_rejected();
  computes_fee_and_slippage_adjusted_best_edge();
  negative_edges_do_not_enter_opportunity_log();
  later_quote_replaces_prior_exchange_state();

  std::cout << "engine_tests: all tests passed\n";
  return EXIT_SUCCESS;
}
