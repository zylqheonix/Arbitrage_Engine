#pragma once

#include "config.hpp"
#include "quote.hpp"

#include <fstream>
#include <optional>

struct EvalResult {
  double net_buy_bn_sell_cb;
  double net_buy_cb_sell_bn;
  double best_net;
  bool fresh;
  std::int64_t cb_age_ms;
  std::int64_t bn_age_ms;
};

class CsvLogger {
 public:
  explicit CsvLogger(CsvConfig const& cfg);
  ~CsvLogger();

  void log(std::optional<QuoteUpdate> const& cb,
           std::optional<QuoteUpdate> const& bn,
           EvalResult const& result);

  void flush();

 private:
  bool enabled_;
  std::ofstream file_;

};
