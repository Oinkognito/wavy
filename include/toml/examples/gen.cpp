#include "../toml/toml_generator.hpp"
#include <iostream>

auto main() -> int
{
  TomlGenerator gen;
  gen.addValueStr("name", "test");
  gen.addValueStr("age", "no age");
  gen.addValueBool("fullscreen", true);
  gen.addValueInt("width", 1920);

  // Creating a non-nested table
  gen.createTable("settings");
  gen.addTableValue("settings", "quality", "high");
  gen.addTableValue("settings", "volume", 80);

  // Save to file
  gen.saveToFile("config.toml");

  std::cout << "TOML file saved successfully!" << std::endl;
  return 0;
}
