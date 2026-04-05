# Arbitrage monitor

C++ **detector-only** tool: live **Coinbase** and **Binance** (Spot WebSocket) quotes, normalized into a shared struct, then a small **engine** estimates **two-way** cross-venue edge after **slippage** and **fees**. Optional **CSV** logging, **JSON config**, and **reconnect with backoff**.

This is **not** a trading or execution system. It does not place orders. Output is for **research, learning, and monitoring**.

---

## What it does

- Connects over **WSS + TLS** (Boost.Beast / Asio).
- **Coinbase**: Exchange WebSocket feed, `ticker` channel (product from config, e.g. `BTC-USD`).
- **Binance**: `bookTicker` stream (symbol from config, e.g. `btcusd` on Binance.US for BTC/USD).
- **Engine**: keeps last quote per venue; if both quotes are **fresh** (within `max_age_ms`), computes:
  - **Buy Binance → sell Coinbase** net (per 1 BTC, in quote currency units).
  - **Buy Coinbase → sell Binance** net.
  - **`best_net`**: the better of the two (often negative when there is no arb).
- **CSV**: one row per `evaluate()` call with prices, both nets, `best_net`, freshness, and quote ages.

---

## Requirements

- **CMake** ≥ 3.16  
- **C++20** compiler  
- **OpenSSL** (TLS)  
- **Boost** with **Asio / Beast** (headers) plus built libraries **`boost_thread`** and **`boost_json`**

On macOS with Homebrew, typical installs:

```bash
brew install cmake openssl boost
```

If CMake cannot find Boost, set `HOMEBREW_PREFIX` or adjust `CMAKE_PREFIX_PATH` (see `CMakeLists.txt`).

---

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Executable: **`build/arbitrage_monitor`**.

---

## Run

From the directory that contains **`config.json`** (or pass an explicit path):

```bash
./build/arbitrage_monitor
./build/arbitrage_monitor /path/to/config.json
```

Stop with **Ctrl+C** (graceful close + CSV flush).

---

## Configuration (`config.json`)

| Section | Purpose |
|--------|---------|
| **`coinbase`** | `host`, `port`, `symbol` (e.g. `BTC-USD`), per-leg `fee_rate`. |
| **`binance`** | List of **`hosts`** (fallback order), `target` (usually `/ws`), `symbol` (lowercase stream name, e.g. `btcusd` for BTC/USD on Binance.US), `fee_rate`. |
| **`engine`** | `slippage_rate`, `max_age_ms` (both quotes must be newer than this to compute nets), `min_edge_threshold` (console arb lines only if `best_net` exceeds this). |
| **`reconnect`** | `initial_delay_ms`, `max_delay_ms`, `backoff_multiplier` after read errors. |
| **`csv`** | `enabled`, `path` (default `arb_log.csv`). |

**Binance.com** may **reject** the WebSocket handshake from some regions (e.g. HTTP **451**). The default host list tries **Binance.US** first when using **`btcusd`**, which matches **USD** on both Coinbase and Binance.US.

**TLS note:** the client sets **ALPN to `http/1.1`** so WebSocket upgrade works on endpoints that would otherwise negotiate HTTP/2.

---

## CSV columns (`arb_log.csv`)

Written when `csv.enabled` is true. **`arb_log.csv`** is listed in **`.gitignore`** so large logs are not committed by default.

| Column | Meaning |
|--------|---------|
| `epoch_ms` | Wall-clock time (ms since Unix epoch) when the row was written. |
| `cb_*` / `bn_*` | Best bid/ask and sizes for Coinbase and Binance (last stored quotes). |
| `net_buy_bn_sell_cb` | Modeled net P&amp;L per 1 BTC: buy Binance (ask + slip), sell Coinbase (bid − slip), minus fees. |
| `net_buy_cb_sell_bn` | Same for buy Coinbase, sell Binance. |
| `best_net` | `max` of the two nets. **0** if either side is missing or **`fresh` is false**. |
| `fresh` | `1` only if **both** quotes exist and each is within **`max_age_ms`** of local receive time. |
| `cb_age_ms` / `bn_age_ms` | Age of each stored quote when the row was written. |

If one venue ticks much faster than the other, you will see many rows with **`fresh = 0`** and **zeros** in the net columns; increase **`max_age_ms`** if you accept wider time skew (at the cost of comparability).

---

## Project layout

```
src/
  main.cpp                 # io_context, TLS, feeds, reconnect, signals
  config.{hpp,cpp}         # JSON → Config
  csv_logger.{hpp,cpp}     # CSV output
  quote.hpp                # QuoteUpdate, Exchange enum
  engine/                  # spread + freshness
  exchanges/               # Coinbase / Binance WSS + shared connect_wss
config.json                # default settings (edit freely)
```

---

## Disclaimer

This software is for **educational purposes** only. It is **not** financial, investment, or trading advice. Cryptocurrency markets involve substantial risk. The authors are not responsible for any losses or decisions you make.
