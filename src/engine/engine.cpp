#include "engine.hpp"

#include <algorithm>
#include <cmath>

Engine::Engine(EngineConfig const& cfg, double coinbase_fee,
               double binance_fee, CsvLogger* csv)
    : coinbase_fee_(coinbase_fee),
      binance_fee_(binance_fee),
      slippage_(cfg.slippage_rate),
      max_age_(cfg.max_age),
      min_edge_(cfg.min_edge_threshold),
      csv_(csv) {}

void Engine::update_quote(QuoteUpdate const& quote) {
  switch (quote.exchange) {
    case Exchange::COINBASE:
      cb_quote_ = quote;
      break;
    case Exchange::BINANCE:
      bn_quote_ = quote;
      break;
  }
}

bool Engine::is_fresh() const {
  if (!cb_quote_ || !bn_quote_) return false;
  auto const now = std::chrono::steady_clock::now();
  return (now - cb_quote_->local_time) <= max_age_ &&
         (now - bn_quote_->local_time) <= max_age_;
}

EvalResult Engine::compute_spread() const {
  EvalResult result{};
  if (!cb_quote_ || !bn_quote_) return result;

  auto const now = std::chrono::steady_clock::now();
  result.cb_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - cb_quote_->local_time)
                    .count();
  result.bn_age_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now - bn_quote_->local_time)
                    .count();
  result.fresh = is_fresh();

  if (!result.fresh) return result;

  QuoteUpdate const& coinbase_quote = *cb_quote_;
  QuoteUpdate const& binance_quote = *bn_quote_;
  double const slip = slippage_;

  // Direction 1: buy Binance (ask), sell Coinbase (bid).
  double const bn_buy = binance_quote.best_ask * (1.0 + slip);
  double const cb_sell = coinbase_quote.best_bid * (1.0 - slip);
  double const gross1 = cb_sell - bn_buy;
  double const fee1 = binance_fee_ * bn_buy + coinbase_fee_ * cb_sell;
  result.net_buy_bn_sell_cb = gross1 - fee1;

  // Direction 2: buy Coinbase (ask), sell Binance (bid).
  double const cb_buy = coinbase_quote.best_ask * (1.0 + slip);
  double const bn_sell = binance_quote.best_bid * (1.0 - slip);
  double const gross2 = bn_sell - cb_buy;
  double const fee2 = coinbase_fee_ * cb_buy + binance_fee_ * bn_sell;
  result.net_buy_cb_sell_bn = gross2 - fee2;

  result.best_net = std::max(result.net_buy_bn_sell_cb, result.net_buy_cb_sell_bn);
  return result;
}

EvalResult Engine::evaluate(QuoteUpdate const& quote) {
  update_quote(quote);
  EvalResult const result = compute_spread();

  if (std::abs(result.best_net) > min_edge_) {
    spread_log_.push_back(result.best_net);
  }

  if (csv_) {
    csv_->log(cb_quote_, bn_quote_, result);
  }

  return result;
}

std::size_t Engine::spread_log_size() const { return spread_log_.size(); }

std::optional<QuoteUpdate> const& Engine::coinbase_quote() const {
  return cb_quote_;
}

std::optional<QuoteUpdate> const& Engine::binance_quote() const {
  return bn_quote_;
}
