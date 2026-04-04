#pragma once

#include <chrono>
#include <cstdint>
#include <string>

enum class Exchange : std::uint8_t {
  BINANCE = 1,
  COINBASE = 2,
};

struct QuoteUpdate {
  Exchange exchange{};
  std::string symbol;

  double best_ask{};
  double best_bid{};

  double bid_qty{};
  double ask_qty{};

  std::int64_t timestamp_ms{};
  std::chrono::steady_clock::time_point local_time{};
};
 