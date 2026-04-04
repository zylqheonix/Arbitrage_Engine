#pragma once

#include "../quote.hpp"

#include <chrono>
#include <cstddef>
#include <optional>
#include <vector>

//spread
double spread_calcution(const QuoteUpdate& CoinBaseQuote, const QuoteUpdate& BinanceQuote);

class Engine {
    public:
    Engine();
    void update_quote(const QuoteUpdate& quote);
    double calculate_spread();
    bool is_fresh() const;
    double evaluate(const QuoteUpdate& quote);
    std::size_t spread_log_size() const;

  private:
    // std::optional = “null” until the first tick from that venue.
    std::optional<QuoteUpdate> CoinBaseQuote = std::nullopt;
    std::optional<QuoteUpdate> BinanceQuote = std::nullopt;
    std::vector<double> spreadLog{};
    const double trade_fee_multiplier = 0.001;
    const double slippage = 0.001;
    const std::chrono::milliseconds max_age = std::chrono::milliseconds(1000);

};

