#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/json.hpp>

#include <chrono>
#include <functional>
#include <iostream>
#include <openssl/ssl.h>
#include <string>

#include "../quote.hpp"

namespace beast = boost::beast;
namespace json = boost::json;
namespace asio = boost::asio;
namespace websocket = beast::websocket;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;

using WssStream = websocket::stream<beast::ssl_stream<beast::tcp_stream>>;

WssStream connect_wss(asio::io_context& ioc,
                      ssl::context& ctx,
                      std::string const& host,
                      std::string const& port,
                      std::string const& target);
