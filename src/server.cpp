#if __cplusplus < 202002L
#error "Wavy-Server requires C++20 or later."
#endif

#include "../include/libwavy-server/server.hpp"

auto main() -> int
{
  try
  {
    logger::init_logging();
    boost::asio::io_context   io_context;
    boost::asio::ssl::context ssl_context(boost::asio::ssl::context::sslv23);

    ssl_context.set_options(
      boost::asio::ssl::context::default_workarounds | boost::asio::ssl::context::no_sslv2 |
      boost::asio::ssl::context::no_sslv3 | boost::asio::ssl::context::single_dh_use);

    ssl_context.use_certificate_file(macros::to_string(macros::SERVER_CERT),
                                     boost::asio::ssl::context::pem);
    ssl_context.use_private_key_file(macros::to_string(macros::SERVER_PRIVATE_KEY),
                                     boost::asio::ssl::context::pem);

    HLS_Server server(io_context, ssl_context, WAVY_SERVER_PORT_NO);
    io_context.run();
  }
  catch (std::exception& e)
  {
    LOG_ERROR << SERVER_LOG << "Exception: " << e.what();
  }

  return 0;
}
