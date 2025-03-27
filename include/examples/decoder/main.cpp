#include "../../decode.hpp"
#include "../../libwavy-common/state.hpp"

auto main() -> int
{
  GlobalState gs;

  std::vector<std::string> ts_segments = gs.transport_segments;

  if (ts_segments.empty())
  {
    av_log(nullptr, AV_LOG_ERROR, "No transport stream segments provided\n");
    return 1;
  }

  MediaDecoder               decoder;
  std::vector<unsigned char> decoded_audio;
  if (!decoder.decode(gs.transport_segments, decoded_audio))
  {
    av_log(nullptr, AV_LOG_ERROR, "Decoding failed\n");
    return 1;
  }

  return 0;
}
