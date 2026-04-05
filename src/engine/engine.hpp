#pragma once

#include "../config.hpp"
#include "../csv_logger.hpp"
#include "../quote.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

class Engine {
 public:
  Engine(EngineConfig const& cfg, double coinbase_fee, double binance_fee,
         CsvLogger* csv);

  EvalResult evaluate(QuoteUpdate const& quote);
  bool is_fresh() const;
  std::size_t spread_log_size() const;

  std::optional<QuoteUpdate> const& coinbase_quote() const;
  std::optional<QuoteUpdate> const& binance_quote() const;

 private:
  void update_quote(QuoteUpdate const& quote);
  EvalResult compute_spread() const;

  std::optional<QuoteUpdate> cb_quote_;
  std::optional<QuoteUpdate> bn_quote_;

  double coinbase_fee_;
  double binance_fee_;
  double slippage_;
  std::chrono::milliseconds max_age_;
  double min_edge_;

  std::vector<double> spread_log_;
  CsvLogger* csv_;
};
