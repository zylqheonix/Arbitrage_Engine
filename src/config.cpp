#include "config.hpp"

#include <boost/json.hpp>

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace json = boost::json;

namespace {

std::string str(json::object const& o, char const* key) {
  return std::string(o.at(key).as_string());
}

double dbl(json::object const& o, char const* key) {
  json::value const& v = o.at(key);
  if (v.is_double()) return v.as_double();
  if (v.is_int64()) return static_cast<double>(v.as_int64());
  if (v.is_uint64()) return static_cast<double>(v.as_uint64());
  throw std::runtime_error(std::string("expected number for key: ") + key);
}

std::int64_t i64(json::object const& o, char const* key) {
  json::value const& v = o.at(key);
  if (v.is_int64()) return v.as_int64();
  if (v.is_uint64()) return static_cast<std::int64_t>(v.as_uint64());
  if (v.is_double()) return static_cast<std::int64_t>(v.as_double());
  throw std::runtime_error(std::string("expected integer for key: ") + key);
}

}

Config load_config(std::string const& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("cannot open config: " + path);
  }
  std::string json_string;
  std::string line;
  while(std::getline(file, line)) {
    json_string += line;
  }
  json::object root = json::parse(json_string).as_object();

  Config cfg{};
  // Coinbase configuration
  auto const& coinbase = root.at("coinbase").as_object();
  cfg.coinbase.endpoint = {str(coinbase, "host"), str(coinbase, "port")};
  cfg.coinbase.symbol = str(coinbase, "symbol");
  cfg.coinbase.fee_rate = dbl(coinbase, "fee_rate");

  // Binance configuration
  auto const& binance = root.at("binance").as_object();
  //note: binance has multiple hosts and 2 diff ports unlike coinbase and sometimes the target host isn't always available/same
  //it has something to do with the region of the user relative to the host. I have yet to fully test the .com hosts
  for (auto const& hosts : binance.at("hosts").as_array()) {
    auto const& host_obj = hosts.as_object();
    cfg.binance.endpoints.push_back(
        {std::string(host_obj.at("host").as_string()),
         std::string(host_obj.at("port").as_string())});
  }
  cfg.binance.target = str(binance, "target");
  cfg.binance.symbol = str(binance, "symbol");
  cfg.binance.fee_rate = dbl(binance, "fee_rate");

  // Engine configuration
  auto const& engine = root.at("engine").as_object();
  cfg.engine.slippage_rate = dbl(engine, "slippage_rate");
  cfg.engine.max_age = std::chrono::milliseconds(i64(engine, "max_age_ms"));
  cfg.engine.min_edge_threshold = dbl(engine, "min_edge_threshold");

  // Reconnect configuration
  auto const& reconnect = root.at("reconnect").as_object();
  cfg.reconnect.initial_delay =
      std::chrono::milliseconds(i64(reconnect, "initial_delay_ms"));
  cfg.reconnect.max_delay =
      std::chrono::milliseconds(i64(reconnect, "max_delay_ms"));
  cfg.reconnect.backoff_multiplier = dbl(reconnect, "backoff_multiplier");

  auto const& csv = root.at("csv").as_object();
  cfg.csv.enabled = csv.at("enabled").as_bool();
  cfg.csv.path = str(csv, "path");

  return cfg;
}
