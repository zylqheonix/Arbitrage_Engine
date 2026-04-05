#pragma once

#include <chrono>
#include <string>
#include <vector>

struct Endpoint {
  std::string host;
  std::string port;
};

struct CoinbaseConfig {
  Endpoint endpoint;
  std::string symbol;
  double fee_rate;
};

struct BinanceConfig {
  std::vector<Endpoint> endpoints;
  std::string target;
  std::string symbol;
  double fee_rate;
};

struct EngineConfig {
  double slippage_rate;
  std::chrono::milliseconds max_age;
  double min_edge_threshold;
};

struct ReconnectConfig {
  std::chrono::milliseconds initial_delay;
  std::chrono::milliseconds max_delay;
  double backoff_multiplier;
};

struct CsvConfig {
  bool enabled;
  std::string path;
};

struct Config {
  CoinbaseConfig coinbase;
  BinanceConfig binance;
  EngineConfig engine;
  ReconnectConfig reconnect;
  CsvConfig csv;
};

Config load_config(std::string const& path);
