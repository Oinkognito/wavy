#include <chrono>
#include <iostream>
#include <libwavy/logger.hpp>

namespace libwavy::log
{

auto strip_ansi(const std::string& input) -> std::string
{
  static const boost::regex ansi_regex(ANSI_REGEX);
  return boost::regex_replace(input, ansi_regex, "");
}

auto get_current_timestamp() -> std::string
{
  using namespace std::chrono;

  const auto        now    = system_clock::now();
  const auto        now_ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
  const std::time_t t      = system_clock::to_time_t(now);
  const std::tm     local  = *std::localtime(&t);

  std::ostringstream oss;
  oss << std::put_time(&local, "%Y-%m-%d %H:%M:%S") << '.' << std::setw(3) << std::setfill('0')
      << now_ms.count();
  return oss.str();
}

void init_logging()
{
  namespace bfs     = boost::filesystem;
  namespace logging = boost::log;
  namespace trivial = boost::log::trivial;
  namespace sinks   = boost::log::sinks;
  namespace expr    = boost::log::expressions;
  namespace kw      = boost::log::keywords;
  using expr::stream;

  using Severity = trivial::severity_level;

  auto L_ConsoleFormatter = []
  {
    return stream << BOLD << "[" << get_current_timestamp() << "] "
                  << expr::if_(expr::attr<Severity>("Severity") ==
                               trivial::trace)[stream << PURPLE << "[TRACE]   "]
                  << expr::if_(expr::attr<Severity>("Severity") ==
                               trivial::info)[stream << GREEN << "[INFO]    "]
                  << expr::if_(expr::attr<Severity>("Severity") ==
                               trivial::warning)[stream << YELLOW << "[WARN]    "]
                  << expr::if_(expr::attr<Severity>("Severity") ==
                               trivial::error)[stream << RED << "[ERROR]   "]
                  << expr::if_(expr::attr<Severity>("Severity") ==
                               trivial::debug)[stream << BLUE << "[DEBUG]   "]
                  << RESET << expr::smessage;
  };

  const char* home;
  GET_HOME_OR_RETURN(home);

  bfs::path log_dir = bfs::path(home) / REL_PATH_LOGS;

  if (!bfs::exists(log_dir))
  {
    if (!bfs::create_directories(log_dir))
    {
      std::cerr << "ERROR: Failed to create log directory: " << log_dir.string() << std::endl;
      return;
    }
  }

  // Define log file path
  const std::string log_file = (log_dir / "wavy_%Y-%m-%d_%H-%M-%S.log").string();

  logging::add_console_log(std::cout, kw::format = L_ConsoleFormatter());

  // File logging (without ANSI codes)
  using text_sink = sinks::synchronous_sink<sinks::text_file_backend>;
  boost::shared_ptr<text_sink> file_sink =
    boost::make_shared<text_sink>(kw::file_name     = log_file,
                                  kw::rotation_size = 10 * 1024 * 1024, // 10 MB
                                  kw::auto_flush    = true);

  // Custom Filtering: Strip ANSI codes before writing logs to a file
  file_sink->set_formatter(
    [](boost::log::record_view const& rec, boost::log::formatting_ostream& strm)
    {
      auto        severity    = rec[trivial::severity];
      auto        message_ref = rec[expr::smessage];
      std::string message     = message_ref ? message_ref.get() : "";

      // Strip ANSI escape codes
      static const boost::regex ansi_regex(ANSI_REGEX);
      message = boost::regex_replace(message, ansi_regex, "");

      strm << "[" << get_current_timestamp() << "] " << (severity ? severity.get() : trivial::info)
           << " " << message;
    });

  boost::log::core::get()->add_sink(file_sink);

  boost::log::add_common_attributes();

  // Set log level from environment variable
  const char* env_level = std::getenv("WAVY_LOG_LEVEL");

  if (env_level)
  {
    std::string level_str = env_level;
    std::ranges::for_each(level_str,
                          [](char& c) { c = std::toupper(static_cast<unsigned char>(c)); });

    auto it = LOG_LEVEL_STR_MAP.find(level_str);
    if (it != LOG_LEVEL_STR_MAP.end())
    {
      set_log_level(it->second);
    }
    else
    {
      std::cerr << "Invalid WAVY_LOG_LEVEL: " << level_str << ". Using default.\n";
    }
  }
  else
  {
    // Default level if no ENV provided
    std::cout << "==> No value found for WAVY_LOG_LEVEL! Falling back to default (INFO)..."
              << std::endl;
    set_log_level(__INFO__);
  }
}

void set_log_level(SeverityLevel level)
{
  namespace trivial = boost::log::trivial;

  auto it = LOG_LEVEL_ENUM_MAP.find(level);
  if (it != LOG_LEVEL_ENUM_MAP.end())
  {
    boost::log::core::get()->set_filter(trivial::severity >= it->second);
    std::cout << "==> Log Level context changed to: " << it->second << "!!" << std::endl;
  }
  else
  {
    std::cerr << "Unknown log level specified.\n";
  }
}

} // namespace libwavy::log
