cmake_minimum_required(VERSION 3.16)
project(arbitrage_monitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(POLICY CMP0167)
  cmake_policy(SET CMP0167 OLD)
endif()

# Optional: help CMake find Homebrew installs on macOS (no-op elsewhere).
if(APPLE AND DEFINED ENV{HOMEBREW_PREFIX})
  list(APPEND CMAKE_PREFIX_PATH
    "$ENV{HOMEBREW_PREFIX}/opt/boost"
    "$ENV{HOMEBREW_PREFIX}/opt/openssl@3"
    "$ENV{HOMEBREW_PREFIX}/opt/openssl"
  )
endif()

find_package(OpenSSL REQUIRED)

# Beast is header-only; Asio uses Boost.System. JSON parsing uses Boost.JSON.
# Homebrew's Boost CMake config can be inconsistent across environments,
# so resolve include/libs explicitly for reliability.
set(_BOOST_HINTS
  /opt/homebrew/opt/boost
  /usr/local/opt/boost
)

find_path(BOOST_INCLUDE_DIR
  NAMES boost/asio.hpp
  HINTS ${_BOOST_HINTS}
  PATH_SUFFIXES include
  REQUIRED
)

find_library(BOOST_THREAD_LIBRARY
  NAMES boost_thread
  HINTS ${_BOOST_HINTS}
  PATH_SUFFIXES lib
  REQUIRED
)

find_library(BOOST_JSON_LIBRARY
  NAMES boost_json
  HINTS ${_BOOST_HINTS}
  PATH_SUFFIXES lib
  REQUIRED
)

set(ARBITRAGE_CORE_SOURCES
  src/config.cpp
  src/csv_logger.cpp
  src/engine/engine.cpp
  src/exchanges/exchange_ws_common.cpp
  src/exchanges/binance_ws.cpp
  src/exchanges/coinbase_ws.cpp
)

add_library(arbitrage_core STATIC ${ARBITRAGE_CORE_SOURCES})

target_include_directories(arbitrage_core
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${BOOST_INCLUDE_DIR}
)

target_link_libraries(arbitrage_core
  PUBLIC
    ${BOOST_THREAD_LIBRARY}
    ${BOOST_JSON_LIBRARY}
    OpenSSL::SSL
    OpenSSL::Crypto
)

if(MSVC)
  target_compile_definitions(arbitrage_core PUBLIC _WIN32_WINNT=0x0A00)
else()
  target_compile_options(arbitrage_core PRIVATE
    -Wall -Wextra -Wpedantic
  )
endif()

add_executable(arbitrage_monitor src/main.cpp)
target_link_libraries(arbitrage_monitor PRIVATE arbitrage_core)

add_executable(engine_tests tests/engine_tests.cpp)
target_link_libraries(engine_tests PRIVATE arbitrage_core)

add_executable(engine_benchmark benchmarks/engine_benchmark.cpp)
target_link_libraries(engine_benchmark PRIVATE arbitrage_core)

enable_testing()
add_test(NAME engine_tests COMMAND engine_tests)
