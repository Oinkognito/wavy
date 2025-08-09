#pragma once
#include <string>
#include <string_view>
#include <vector>

struct Part
{
  std::string headers;
  std::string content;
  std::string name;
  std::string filename;
  std::string content_type;
};

class MultipartParser
{
public:
  explicit MultipartParser(std::string_view boundary);
  auto parse(const std::string& body) -> std::vector<Part>;

private:
  std::string_view boundary;
};
