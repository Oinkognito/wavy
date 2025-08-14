#include <fstream>
#include <libwavy/network/entry.hpp>

auto main(int argc, char* argv[]) -> int
{
  INIT_WAVY_LOGGER_ALL();

  const IPAddr     server_ip = argv[1];
  const NetTarget  route     = argv[2];
  asio::io_context ioc;
  ssl::context     ctx(ssl::context::tlsv12_client);
  ctx.set_verify_mode(ssl::verify_none);
  libwavy::network::HttpsClient client(ioc, ctx, server_ip);

  std::ofstream out_file("output.bin", std::ios::binary);
  if (!out_file)
  {
    lwlog::ERROR<_>("Failed to open output file for writing");
    return 1;
  }

  client.get_chunked(route,
                     [&out_file](const std::string& chunk)
                     {
                       out_file.write(chunk.data(), chunk.size());
                       lwlog::INFO<_>("Wrote {} bytes to file", chunk.size());
                     });

  lwlog::INFO<_>("Chunked transfer finished!");
  return 0;
}
