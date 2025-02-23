#include "ABRManager.hpp"
#include <boost/asio.hpp>

auto main(int argc, char* argv[]) -> int
{
  if (argc != 2)
  {
    std::cerr << "Usage: " << argv[0] << " <network-stream>" << std::endl;
    return EXIT_FAILURE;
  }
  try
  {
    boost::asio::io_context ioc;

    // Replace with your HLS master playlist URL
    std::string master_url = argv[1];

    ABRManager abr_manager(ioc, master_url);
    abr_manager.selectBestBitrate();
  }
  catch (const std::exception& e)
  {
    std::cerr << "[ERROR] Exception: " << e.what() << "\n";
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
