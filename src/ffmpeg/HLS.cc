/********************************************************************************
 *                                Wavy Project                                  *
 *                         High-Fidelity Audio Streaming                        *
 *                                                                              *
 *  Copyright (c) 2025 Oinkognito                                               *
 *  All rights reserved.                                                        *
 *                                                                              *
 *  License:                                                                    *
 *  This software is licensed under the BSD-3-Clause License. You may use,      *
 *  modify, and distribute this software under the conditions stated in the     *
 *  LICENSE file provided in the project root.                                  *
 *                                                                              *
 *  Warranty Disclaimer:                                                        *
 *  This software is provided "AS IS", without any warranties or guarantees,    *
 *  either expressed or implied, including but not limited to fitness for a     *
 *  particular purpose.                                                         *
 *                                                                              *
 *  Contributions:                                                              *
 *  Contributions are welcome. By submitting code, you agree to license your    *
 *  contributions under the same BSD-3-Clause terms.                            *
 *                                                                              *
 *  See LICENSE file for full legal details.                                    *
 ********************************************************************************/

#include <filesystem>
#include <fstream>
#include <libwavy/ffmpeg/hls/entry.hpp>
#include <regex>

namespace fs = std::filesystem;
using HLS    = libwavy::log::HLS;

namespace libwavy::ffmpeg::hls
{

HLS_Segmenter::HLS_Segmenter() { avformat_network_init(); }

HLS_Segmenter::~HLS_Segmenter() { avformat_network_deinit(); }

auto HLS_Segmenter::createSegmentsFLAC(const AbsPath& input_file, const Directory& output_dir,
                                       CStrRelPath output_playlist, int bitrate) -> bool
{
  AVFormatContext *input_ctx = nullptr, *output_ctx = nullptr;
  AVStream *       in_stream = nullptr, *out_stream = nullptr;
  AVPacket*        pkt = nullptr;
  int              ret;
  AudioStreamIdx   audio_stream_idx    = -1;
  const AbsPath    segment_file_format = output_dir + "/hls_flac_%d.m4s";
  const AbsPath    output_playlist_str = output_dir + "/" + output_playlist;

  log::DBG<HLS>("Segments format: {}", segment_file_format);
  log::DBG<HLS>("Playlist destination: {}", output_playlist_str);

  if ((ret = avformat_open_input(&input_ctx, input_file.c_str(), nullptr, nullptr)) < 0)
    return ret;

  if ((ret = avformat_find_stream_info(input_ctx, nullptr)) < 0)
    goto cleanup;

  for (int i = 0; i < input_ctx->nb_streams; i++)
  {
    if (input_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      audio_stream_idx = i;
      break;
    }
  }

  if (audio_stream_idx == -1)
  {
    ret = AVERROR(EINVAL);
    goto cleanup;
  }

  avformat_alloc_output_context2(&output_ctx, nullptr, "hls", output_playlist_str.c_str());
  if (!output_ctx)
  {
    ret = AVERROR_UNKNOWN;
    goto cleanup;
  }

  av_opt_set(output_ctx->priv_data, "hls_segment_type", "fmp4", 0);
  av_opt_set(output_ctx->priv_data, "hls_playlist_type", "vod", 0);
  av_opt_set(output_ctx->priv_data,
             macros::to_string(macros::CODEC_HLS_SEGMENT_FILENAME_FIELD).c_str(),
             segment_file_format.c_str(), 0);
  av_opt_set(output_ctx->priv_data, "master_pl_name", macros::to_cstr(macros::MASTER_PLAYLIST), 0);

  out_stream = avformat_new_stream(output_ctx, nullptr);
  if (!out_stream)
  {
    ret = AVERROR_UNKNOWN;
    goto cleanup;
  }

  in_stream = input_ctx->streams[audio_stream_idx];
  if ((ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar)) < 0)
    goto cleanup;

  out_stream->codecpar->bit_rate = bitrate;

  if (!(output_ctx->oformat->flags & AVFMT_NOFILE))
  {
    if ((ret = avio_open(&output_ctx->pb, output_ctx->url, AVIO_FLAG_WRITE)) < 0)
      goto cleanup;
  }

  if ((ret = avformat_write_header(output_ctx, nullptr)) < 0)
    goto cleanup;

  pkt = av_packet_alloc();
  if (!pkt)
  {
    ret = AVERROR(ENOMEM);
    goto cleanup;
  }

  while (av_read_frame(input_ctx, pkt) >= 0)
  {
    if (pkt->stream_index == audio_stream_idx)
    {
      pkt->pts =
        av_rescale_q_rnd(pkt->pts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
      pkt->dts =
        av_rescale_q_rnd(pkt->dts, in_stream->time_base, out_stream->time_base, AV_ROUND_NEAR_INF);
      pkt->duration     = av_rescale_q(pkt->duration, in_stream->time_base, out_stream->time_base);
      pkt->pos          = -1;
      pkt->stream_index = 0;

      if ((ret = av_interleaved_write_frame(output_ctx, pkt)) < 0)
        break;
    }
    av_packet_unref(pkt);
  }

  av_write_trailer(output_ctx);

cleanup:
  if (pkt)
    av_packet_free(&pkt);
  if (output_ctx && !(output_ctx->oformat->flags & AVFMT_NOFILE))
    avio_closep(&output_ctx->pb);
  if (output_ctx)
    avformat_free_context(output_ctx);
  if (input_ctx)
    avformat_close_input(&input_ctx);

  return ret >= 0;
}

auto HLS_Segmenter::createSegments(CStrRelPath input_file, CStrDirectory output_dir, bool use_flac)
  -> std::vector<int>
{
  std::vector<std::string> playlist_files;

  int bitrate = lbwMetadata.fetchBitrate(input_file);
  found_bitrates.emplace_back(bitrate);

  std::string codec_prefix    = use_flac ? "flac" : "mp3";
  std::string output_playlist = std::string(output_dir) + "/hls_" + codec_prefix + "_" +
                                std::to_string(bitrate) + macros::to_string(macros::PLAYLIST_EXT);
  playlist_files.push_back(output_playlist);

  bool success = use_flac
                   ? createSegmentsFLAC(input_file, output_dir, output_playlist.c_str(), bitrate)
                   : encode_variant(input_file, output_playlist.c_str(), bitrate);

  if (!success)
  {
    av_log(nullptr, AV_LOG_ERROR, "Encoding failed for file %s\n", input_file);
    return {};
  }

  return found_bitrates;
}

void HLS_Segmenter::createMasterPlaylistMP3(const Directory& input_dir, const Directory& output_dir)
{
  std::vector<std::string> playlists;
  std::vector<int>         bitrates;

  std::regex bitrate_regex(R"(hls_mp3_(\d+)\.m3u8$)");

  for (const auto& entry : std::filesystem::directory_iterator(input_dir))
  {
    if (entry.is_regular_file() && entry.path().extension() == macros::PLAYLIST_EXT)
    {
      std::string filename = entry.path().filename().string();
      std::smatch match;
      if (std::regex_search(filename, match, bitrate_regex))
      {
        playlists.push_back(filename);
        bitrates.push_back(std::stoi(match[1]));
      }
    }
  }

  if (playlists.empty())
  {
    log::ERROR<HLS>("No playlists found in directory: {}", input_dir);
    return;
  }

  const AbsPath master_playlist = output_dir + "/" + macros::to_string(macros::MASTER_PLAYLIST);
  std::ofstream m3u8(master_playlist);

  if (!m3u8)
  {
    av_log(nullptr, AV_LOG_ERROR, "Failed to create master playlist\n");
    return;
  }

  m3u8 << macros::MASTER_PLAYLIST_HEADER;

  for (size_t i = 0; i < playlists.size(); i++)
  {
    m3u8 << "#EXT-X-STREAM-INF:BANDWIDTH=" << bitrates[i] << "," << macros::MP3_CODEC << "\n";
    m3u8 << playlists[i] << "\n";
  }

  m3u8.close();

  log::INFO<HLS>("Created HLS segments for LOSSY with references written to master playlist: {}",
                 macros::to_string(macros::MASTER_PLAYLIST));
}

auto HLS_Segmenter::encode_variant(CStrRelPath input_file, CStrRelPath output_playlist, int bitrate)
  -> bool
{
  AVFormatContext* input_ctx          = nullptr;
  AVFormatContext* output_ctx         = nullptr;
  AVStream*        audio_stream       = nullptr;
  AudioStreamIdx   audio_stream_index = -1;
  AVDictionary*    options            = nullptr;

  if (avformat_open_input(&input_ctx, input_file, nullptr, nullptr) < 0)
    return false;

  if (avformat_find_stream_info(input_ctx, nullptr) < 0)
  {
    avformat_close_input(&input_ctx);
    return false;
  }

  for (AudioStreamIdxIter i = 0; i < input_ctx->nb_streams; i++)
  {
    if (input_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
    {
      audio_stream_index = i;
      break;
    }
  }

  if (audio_stream_index == -1)
  {
    avformat_close_input(&input_ctx);
    return false;
  }

  if (avformat_alloc_output_context2(&output_ctx, nullptr, "hls", output_playlist) < 0)
  {
    avformat_close_input(&input_ctx);
    return false;
  }

  audio_stream = avformat_new_stream(output_ctx, nullptr);
  if (!audio_stream)
  {
    avformat_close_input(&input_ctx);
    avformat_free_context(output_ctx);
    return false;
  }

  if (avcodec_parameters_copy(audio_stream->codecpar,
                              input_ctx->streams[audio_stream_index]->codecpar) < 0)
  {
    avformat_close_input(&input_ctx);
    avformat_free_context(output_ctx);
    return false;
  }

  audio_stream->codecpar->bit_rate = bitrate * 1000;

  RelPath   output_playlist_str = output_playlist;
  size_t    last_slash          = output_playlist_str.find_last_of('/');
  Directory out_dir =
    (last_slash != std::string::npos) ? output_playlist_str.substr(0, last_slash) : ".";
  AbsPath segment_filename_format = out_dir + "/hls_mp3_" + std::to_string(bitrate) + "_%d.ts";

  av_dict_set(&options, macros::to_string(macros::CODEC_HLS_TIME_FIELD).c_str(), "10", 0);
  av_dict_set(&options, macros::to_string(macros::CODEC_HLS_LIST_SIZE_FIELD).c_str(), "0", 0);
  av_dict_set(&options, macros::to_string(macros::CODEC_HLS_FLAGS_FIELD).c_str(),
              "independent_segments", 0);
  av_dict_set(&options, macros::to_string(macros::CODEC_HLS_SEGMENT_FILENAME_FIELD).c_str(),
              segment_filename_format.c_str(), 0);

  if (avformat_write_header(output_ctx, &options) < 0)
  {
    av_dict_free(&options);
    avformat_close_input(&input_ctx);
    avformat_free_context(output_ctx);
    return false;
  }

  AVPacket pkt;
  while (av_read_frame(input_ctx, &pkt) >= 0)
  {
    if (pkt.stream_index == audio_stream_index)
    {
      pkt.stream_index = audio_stream->index;
      pkt.pts          = av_rescale_q(pkt.pts, input_ctx->streams[audio_stream_index]->time_base,
                                      audio_stream->time_base);
      pkt.dts          = av_rescale_q(pkt.dts, input_ctx->streams[audio_stream_index]->time_base,
                                      audio_stream->time_base);
      pkt.duration = av_rescale_q(pkt.duration, input_ctx->streams[audio_stream_index]->time_base,
                                  audio_stream->time_base);

      if (av_interleaved_write_frame(output_ctx, &pkt) < 0)
      {
        av_packet_unref(&pkt);
        break;
      }
    }
    av_packet_unref(&pkt);
  }

  av_write_trailer(output_ctx);

  av_dict_free(&options);
  avformat_close_input(&input_ctx);
  if (output_ctx)
  {
    if (!(output_ctx->oformat->flags & AVFMT_NOFILE))
      avio_closep(&output_ctx->pb);
    avformat_free_context(output_ctx);
  }

  return true;
}

} // namespace libwavy::ffmpeg::hls
