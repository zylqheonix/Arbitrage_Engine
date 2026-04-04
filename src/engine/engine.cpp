#include "engine.hpp"

#include <algorithm>

Engine::Engine() = default;

void Engine::update_quote(const QuoteUpdate& quote) {
  switch (quote.exchange) {
    case Exchange::BINANCE:
      BinanceQuote = quote;
      break;
    case Exchange::COINBASE:
      CoinBaseQuote = quote;
      break;
    default:
      break;
  }
}

double Engine::calculate_spread() {
  if (!CoinBaseQuote || !BinanceQuote) {
    return 0.0;
  }
  if (!is_fresh()) {
    return 0.0;
  }

  QuoteUpdate const& cb = *CoinBaseQuote;
  QuoteUpdate const& bn = *BinanceQuote;

  // Slippage: worse buy (higher), worse sell (lower).
  double const slip = slippage;

  // Direction 1 — buy Binance (ask), sell Coinbase (bid).
  double const bn_buy = bn.best_ask * (1.0 + slip);
  double const cb_sell = cb.best_bid * (1.0 - slip);
  double const gross_buy_bn_sell_cb = cb_sell - bn_buy;
  double const fee_leg1 =
      trade_fee_multiplier * (bn_buy + cb_sell);
  double const net1 = gross_buy_bn_sell_cb - fee_leg1;

  // Direction 2 — buy Coinbase (ask), sell Binance (bid).
  double const cb_buy = cb.best_ask * (1.0 + slip);
  double const bn_sell = bn.best_bid * (1.0 - slip);
  double const gross_buy_cb_sell_bn = bn_sell - cb_buy;
  double const fee_leg2 =
      trade_fee_multiplier * (cb_buy + bn_sell);
  double const net2 = gross_buy_cb_sell_bn - fee_leg2;
  if (net1 < 0.0 && net2 < 0.0) {
    return 0.0;
  } else {
    return std::max(net1, net2);
  }
}

double Engine::evaluate(const QuoteUpdate& quote) {
  update_quote(quote);
  double const spread = calculate_spread();
  if (spread != 0.0) {
    spreadLog.push_back(spread);
  }
  return spread;
}

std::size_t Engine::spread_log_size() const {
  return spreadLog.size();
}

bool Engine::is_fresh() const {
  if (!CoinBaseQuote || !BinanceQuote) {
    return false;
  }
  auto const now = std::chrono::steady_clock::now();
  auto const age_cb = now - CoinBaseQuote->local_time;
  auto const age_bn = now - BinanceQuote->local_time;
  return age_cb <= max_age && age_bn <= max_age;
}