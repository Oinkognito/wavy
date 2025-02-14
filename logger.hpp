#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/sinks.hpp>
#include <boost/log/sinks/text_file_backend.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>
#include <boost/log/utility/setup/settings.hpp>
#include <iostream>

namespace logger
{
enum SeverityLevel
{
  INFO,
  WARNING,
  ERROR,
  DEBUG
};

inline void init_logging()
{
  // Log to console
  boost::log::add_console_log(std::cout, boost::log::keywords::format = "[%TimeStamp%] %Message%");

  // Log to file with rotation
  boost::log::add_file_log(boost::log::keywords::file_name = "hls_server_%N.log",
                           boost::log::keywords::rotation_size =
                             5 * 1024 * 1024, // 5MB per log file
                           boost::log::keywords::auto_flush = true,
                           boost::log::keywords::format    = "[%TimeStamp%] [%Severity%] %Message%",
                           boost::log::keywords::open_mode = std::ios_base::app);

  boost::log::add_common_attributes();
}

#define LOG_INFO    BOOST_LOG_TRIVIAL(info)
#define LOG_WARNING BOOST_LOG_TRIVIAL(warning)
#define LOG_ERROR   BOOST_LOG_TRIVIAL(error)
#define LOG_DEBUG   BOOST_LOG_TRIVIAL(debug)

} // namespace logger

#endif // LOGGER_HPP
