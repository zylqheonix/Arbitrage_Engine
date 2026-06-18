#include "../src/engine/engine.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct BenchmarkResult {
  std::string name;
  std::size_t iterations{};
  std::size_t fresh_results{};
  std::size_t opportunity_samples{};
  double checksum{};
  double seconds{};
  double evals_per_second{};
  double mean_ns{};
  long long p50_ns{};
  long long p95_ns{};
  long long p99_ns{};
};

EngineConfig bench_config(double min_edge_threshold) {
  return EngineConfig{
      .slippage_rate = 0.001,
      .max_age = std::chrono::hours(1),
      .min_edge_threshold = min_edge_threshold,
  };
}

QuoteUpdate quote(Exchange exchange, double bid, double ask,
                  Clock::time_point local_time) {
  return QuoteUpdate{
      .exchange = exchange,
      .symbol = "BTC-USD",
      .best_ask = ask,
      .best_bid = bid,
      .bid_qty = 1.0,
      .ask_qty = 1.0,
      .timestamp_ms = 0,
      .local_time = local_time,
  };
}

std::vector<QuoteUpdate> make_balanced_quotes(std::size_t count) {
  std::vector<QuoteUpdate> quotes;
  quotes.reserve(count);
  auto const now = Clock::now();

  for (std::size_t i = 0; i < count; ++i) {
    double const micro_move = static_cast<double>(i % 17) * 0.01;
    if ((i % 2) == 0) {
      quotes.push_back(quote(Exchange::COINBASE, 30000.00 + micro_move,
                             30000.50 + micro_move, now));
    } else {
      quotes.push_back(quote(Exchange::BINANCE, 30000.02 + micro_move,
                             30000.52 + micro_move, now));
    }
  }

  return quotes;
}

std::vector<QuoteUpdate> make_crossed_quotes(std::size_t count) {
  std::vector<QuoteUpdate> quotes;
  quotes.reserve(count);
  auto const now = Clock::now();

  for (std::size_t i = 0; i < count; ++i) {
    double const micro_move = static_cast<double>(i % 11) * 0.02;
    if ((i % 2) == 0) {
      quotes.push_back(quote(Exchange::COINBASE, 30000.00 + micro_move,
                             30000.25 + micro_move, now));
    } else {
      quotes.push_back(quote(Exchange::BINANCE, 30125.00 + micro_move,
                             30125.25 + micro_move, now));
    }
  }

  return quotes;
}

long long percentile(std::vector<long long>& samples, double pct) {
  if (samples.empty()) return 0;
  std::sort(samples.begin(), samples.end());
  std::size_t const index = static_cast<std::size_t>(
      (static_cast<double>(samples.size() - 1) * pct) / 100.0);
  return samples[index];
}

BenchmarkResult run_benchmark(std::string name,
                              std::vector<QuoteUpdate> const& quotes,
                              double min_edge_threshold) {
  constexpr double kCoinbaseFee = 0.001;
  constexpr double kBinanceFee = 0.001;
  constexpr std::size_t kWarmupIterations = 10000;
  constexpr std::size_t kLatencySampleStride = 512;

  Engine warmup(bench_config(min_edge_threshold), kCoinbaseFee, kBinanceFee,
                nullptr);
  for (std::size_t i = 0; i < std::min(kWarmupIterations, quotes.size()); ++i) {
    warmup.evaluate(quotes[i]);
  }

  Engine engine(bench_config(min_edge_threshold), kCoinbaseFee, kBinanceFee,
                nullptr);
  std::vector<long long> latency_samples;
  latency_samples.reserve(quotes.size() / kLatencySampleStride + 1);

  BenchmarkResult result;
  result.name = std::move(name);
  result.iterations = quotes.size();

  auto const started = Clock::now();
  for (std::size_t i = 0; i < quotes.size(); ++i) {
    if ((i % kLatencySampleStride) == 0) {
      auto const sample_started = Clock::now();
      EvalResult const eval = engine.evaluate(quotes[i]);
      auto const sample_finished = Clock::now();
      latency_samples.push_back(
          std::chrono::duration_cast<std::chrono::nanoseconds>(
              sample_finished - sample_started)
              .count());
      result.checksum += eval.best_net;
      result.fresh_results += eval.fresh ? 1U : 0U;
    } else {
      EvalResult const eval = engine.evaluate(quotes[i]);
      result.checksum += eval.best_net;
      result.fresh_results += eval.fresh ? 1U : 0U;
    }
  }
  auto const finished = Clock::now();

  result.seconds = std::chrono::duration<double>(finished - started).count();
  result.evals_per_second =
      static_cast<double>(result.iterations) / result.seconds;
  result.opportunity_samples = engine.spread_log_size();
  result.mean_ns =
      std::accumulate(latency_samples.begin(), latency_samples.end(), 0.0) /
      static_cast<double>(latency_samples.size());
  result.p50_ns = percentile(latency_samples, 50.0);
  result.p95_ns = percentile(latency_samples, 95.0);
  result.p99_ns = percentile(latency_samples, 99.0);

  return result;
}

void print_result(BenchmarkResult const& result) {
  std::cout << std::fixed << std::setprecision(2);
  std::cout << result.name << "\n";
  std::cout << "  iterations: " << result.iterations << "\n";
  std::cout << "  fresh results: " << result.fresh_results << "\n";
  std::cout << "  opportunity samples: " << result.opportunity_samples << "\n";
  std::cout << "  elapsed seconds: " << result.seconds << "\n";
  std::cout << "  throughput eval/s: " << result.evals_per_second << "\n";
  std::cout << "  sampled latency ns mean/p50/p95/p99: " << result.mean_ns << "/"
            << result.p50_ns << "/" << result.p95_ns << "/" << result.p99_ns
            << "\n";
  std::cout << "  checksum: " << result.checksum << "\n";
}

std::size_t parse_iterations(int argc, char* argv[]) {
  if (argc < 2) return 5000000;

  long long const parsed = std::atoll(argv[1]);
  if (parsed <= 0) {
    std::cerr << "usage: engine_benchmark [positive_iterations]\n";
    std::exit(EXIT_FAILURE);
  }

  return static_cast<std::size_t>(parsed);
}

}  // namespace

int main(int argc, char* argv[]) {
  std::size_t const iterations = parse_iterations(argc, argv);

  std::cout << "engine_benchmark: synthetic offline quote streams, CSV disabled\n";
  std::cout << "iterations per scenario: " << iterations << "\n\n";

  BenchmarkResult const balanced = run_benchmark(
      "balanced_market_no_opportunity_log", make_balanced_quotes(iterations),
      1000000.0);
  print_result(balanced);

  std::cout << "\n";

  BenchmarkResult const crossed = run_benchmark(
      "crossed_market_with_opportunity_log", make_crossed_quotes(iterations),
      0.10);
  print_result(crossed);

  std::cout << "\nresume summary: evaluated "
            << (iterations * 2) << " synthetic quote updates across two engine "
            << "scenarios; peak throughput "
            << std::max(balanced.evals_per_second, crossed.evals_per_second)
            << " eval/s with p99 sampled latency "
            << std::min(balanced.p99_ns, crossed.p99_ns) << "-"
            << std::max(balanced.p99_ns, crossed.p99_ns) << " ns.\n";

  return EXIT_SUCCESS;
}
