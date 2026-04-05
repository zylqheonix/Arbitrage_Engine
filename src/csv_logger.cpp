#include "csv_logger.hpp"

#include <iomanip>
#include <iostream>

CsvLogger::CsvLogger(CsvConfig const& cfg) : enabled_(cfg.enabled) {
  if (!enabled_) return;
  file_.open(cfg.path, std::ios::out | std::ios::trunc);
  if (!file_) {
    std::cerr << "[csv] failed to open " << cfg.path << "; disabling CSV.\n";
    enabled_ = false;
    return;
  }
  file_ << "epoch_ms"
        << ",cb_bid,cb_ask,cb_bid_sz,cb_ask_sz"
        << ",bn_bid,bn_ask,bn_bid_sz,bn_ask_sz"
        << ",net_buy_bn_sell_cb,net_buy_cb_sell_bn,best_net"
        << ",fresh,cb_age_ms,bn_age_ms\n";
}

CsvLogger::~CsvLogger() { flush(); }

void CsvLogger::flush() {
  if (enabled_ && file_.is_open()) {
    file_.flush();
  }
}

void CsvLogger::log(std::optional<QuoteUpdate> const& cb,
                    std::optional<QuoteUpdate> const& bn,
                    EvalResult const& r) {
  if (!enabled_) return;

  auto const now = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::system_clock::now().time_since_epoch())
                       .count();

  file_ << std::fixed << std::setprecision(6);
  file_ << now;

  if (cb) {
    file_ << "," << cb->best_bid << "," << cb->best_ask << "," << cb->bid_qty
          << "," << cb->ask_qty;
  } else {
    file_ << ",,,,";
  }

  if (bn) {
    file_ << "," << bn->best_bid << "," << bn->best_ask << "," << bn->bid_qty
          << "," << bn->ask_qty;
  } else {
    file_ << ",,,,";
  }

  file_ << "," << r.net_buy_bn_sell_cb << "," << r.net_buy_cb_sell_bn << ","
        << r.best_net << "," << (r.fresh ? 1 : 0) << "," << r.cb_age_ms << ","
        << r.bn_age_ms << "\n";
}
