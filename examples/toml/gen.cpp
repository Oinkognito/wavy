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

#include <iostream>
#include <libwavy/toml/toml_generator.hpp>

using namespace libwavy::Toml;

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

  gen.addTableArray("settings", "pixels", {1, 3, 4, 5});

  // Save to file
  gen.saveToFile("config.toml");

  std::cout << "TOML file saved successfully!" << std::endl;
  return 0;
}
