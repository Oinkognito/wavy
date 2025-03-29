#include "../../include/libwavy-ffmpeg/hls/entry.hpp"

// This will create a singular mp3 file's HLS segments

auto main(int argc, char* argv[]) -> int
{
  logger::init_logging();
  try
  {
    if (argc > 2)
    {
      libwavy::hls::HLS_Segmenter seg;
      seg.create_hls_segments(argv[1], argv[2]);
    }
    else
    {
      LOG_ERROR << argv[0] << " <input-file> " << "<output-dir>";
      return 1;
    }
  }
  catch (const std::exception& e)
  {
    LOG_ERROR << "Something went wrong!";
    return 1;
  }

  return 0;
}
