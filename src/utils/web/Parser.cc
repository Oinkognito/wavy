#include <cstring>
#include <libwavy/utils/web/parser.hpp>
#include <sstream>
#include <string>

MultipartParser::MultipartParser(std::string_view b) : boundary(b) {}

auto MultipartParser::parse(const std::string& body) -> std::vector<Part>
{
  std::vector<Part> parts;
  const std::string delimiter = "--" + std::string(boundary);

  size_t pos = 0;
  while ((pos = body.find(delimiter, pos)) != std::string::npos)
  {
    pos += delimiter.length();
    if (body.substr(pos, 2) == "--")
      break; // End of parts

    size_t header_end = body.find("\r\n\r\n", pos);
    if (header_end == std::string::npos)
      break;

    std::string headers       = body.substr(pos, header_end - pos);
    size_t      content_start = header_end + 4;
    size_t      content_end   = body.find(delimiter, content_start);
    if (content_end == std::string::npos)
      break;

    std::string content = body.substr(content_start, content_end - content_start);
    Part        part{.headers = headers, .content = content};

    std::istringstream hs(headers);
    std::string        line;
    while (std::getline(hs, line))
    {
      if (line.starts_with("Content-Disposition:"))
      {
        size_t n = line.find("name=\"");
        size_t f = line.find("filename=\"");
        if (n != std::string::npos)
          part.name = line.substr(n + 6, line.find("\"", n + 6) - (n + 6));
        if (f != std::string::npos)
          part.filename = line.substr(f + 10, line.find("\"", f + 10) - (f + 10));
      }
      if (line.starts_with("Content-Type:"))
        part.content_type = line.substr(strlen("Content-Type: "));
    }

    parts.push_back(part);
    pos = content_end;
  }
  return parts;
}
