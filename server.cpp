#include "logger.hpp"
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <fstream>

using boost::asio::ip::tcp;

const std::string HLS_DIR = "/home/s1dd/dev/wavy/"; // HLS files directory

class HLS_Session : public std::enable_shared_from_this<HLS_Session>
{
public:
  explicit HLS_Session(boost::asio::ssl::stream<tcp::socket> socket) : socket_(std::move(socket)) {}

  void start()
  {
    LOG_INFO << "Starting new session";
    do_handshake();
  }

private:
  boost::asio::ssl::stream<tcp::socket> socket_;
  boost::asio::streambuf                request_;
  std::string                           response_;

  void do_handshake()
  {
    auto self(shared_from_this());
    socket_.async_handshake(boost::asio::ssl::stream_base::server,
                            [this, self](boost::system::error_code ec)
                            {
                              if (ec)
                              {
                                LOG_ERROR << "SSL handshake failed: " << ec.message();
                                return;
                              }
                              LOG_INFO << "SSL handshake successful";
                              do_read();
                            });
  }

  void do_read()
  {
    auto self(shared_from_this());
    boost::asio::async_read_until(socket_, request_, "\r\n\r\n",
                                  [this, self](boost::system::error_code ec, std::size_t bytes_transferred)
                                  {
                                    if (ec)
                                    {
                                      LOG_ERROR << "Read error: " << ec.message();
                                      return;
                                    }
                                    LOG_INFO << "Received " << bytes_transferred << " bytes";
                                    process_request();
                                  });
  }

  void process_request()
  {
    std::istream request_stream(&request_);
    std::string  method, path, version;
    request_stream >> method >> path >> version;

    LOG_INFO << "Client requested: " << method << " " << path;

    if (method != "GET")
    {
      LOG_WARNING << "Unsupported HTTP method: " << method;
      send_response("HTTP/1.1 405 Method Not Allowed\r\n\r\n");
      return;
    }

    std::string   file_path = HLS_DIR + path;
    std::ifstream file(file_path, std::ios::binary);

    if (!file)
    {
      LOG_ERROR << "File not found: " << file_path;
      send_response("HTTP/1.1 404 Not Found\r\n\r\n");
      return;
    }

    std::ostringstream file_content;
    file_content << file.rdbuf();
    std::string content = file_content.str();

    LOG_INFO << "Serving file: " << file_path;
    std::string header =
      "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(content.size()) + "\r\n\r\n";
    response_ = header + content;

    do_write();
  }

  void send_response(const std::string& msg)
  {
    LOG_INFO << "Sending response: " << msg.substr(0, msg.find("\r\n"));
    response_ = msg;
    do_write();
  }

  void do_write()
  {
    auto self(shared_from_this());
    boost::asio::async_write(socket_, boost::asio::buffer(response_),
                             [this, self](boost::system::error_code ec, std::size_t bytes_transferred)
                             {
                               if (ec)
                               {
                                 LOG_ERROR << "Write error: " << ec.message();
                                 return;
                               }
                               LOG_INFO << "Sent " << bytes_transferred << " bytes";
                               socket_.shutdown();
                             });
  }
};

class HLS_Server
{
public:
  HLS_Server(boost::asio::io_context& io_context, boost::asio::ssl::context& ssl_context, short port)
      : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)), ssl_context_(ssl_context)
  {
    LOG_INFO << "Starting HLS server on port " << port;
    start_accept();
  }

private:
  tcp::acceptor              acceptor_;
  boost::asio::ssl::context& ssl_context_;

  void start_accept()
  {
    acceptor_.async_accept(
      [this](boost::system::error_code ec, tcp::socket socket)
      {
        if (ec)
        {
          LOG_ERROR << "Accept failed: " << ec.message();
          return;
        }

        LOG_INFO << "Accepted new connection";
        auto session = std::make_shared<HLS_Session>(
          boost::asio::ssl::stream<tcp::socket>(std::move(socket), ssl_context_));
        session->start();
        start_accept();
      });
  }
};

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

    ssl_context.use_certificate_file("server.crt", boost::asio::ssl::context::pem);
    ssl_context.use_private_key_file("server.key", boost::asio::ssl::context::pem);

    HLS_Server server(io_context, ssl_context, 8080);
    io_context.run();
  }
  catch (std::exception& e)
  {
    LOG_ERROR << "Exception: " << e.what();
  }

  return 0;
}
