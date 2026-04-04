#include "exchange_ws_common.hpp"

WssStream connect_wss(asio::io_context& ioc,
                      ssl::context& ctx,
                      std::string const& host,
                      std::string const& port,
                      std::string const& target) {
  tcp::resolver resolver(ioc);
  WssStream ws{ioc, ctx};

  auto const results = resolver.resolve(host, port);
  beast::get_lowest_layer(ws).connect(results);

  if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host.c_str())) {
    throw beast::system_error(beast::error_code(
        static_cast<int>(::ERR_get_error()), asio::error::get_ssl_category()));
  }

  ws.next_layer().handshake(ssl::stream_base::client);

  // RFC 9112: Host must include :port when not the default (443 for WSS).
  std::string host_field = host;
  if (port != "443" && port != "80" && !port.empty()) {
    host_field += ':';
    host_field += port;
  }
  ws.handshake(host_field, target);
  return ws;
}
