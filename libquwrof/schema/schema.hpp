#pragma once
/************************************************
 * Wavy Project - High-Fidelity Audio Streaming
 * ---------------------------------------------
 * 
 * Copyright (c) 2025 Oinkognito
 * All rights reserved.
 * 
 * This source code is part of the Wavy project, an advanced
 * local networking solution for high-quality audio streaming.
 * 
 * License:
 * This software is licensed under the BSD-3-Clause License.
 * You may use, modify, and distribute this software under
 * the conditions stated in the LICENSE file provided in the
 * project root.
 * 
 * Warranty Disclaimer:
 * This software is provided "AS IS," without any warranties
 * or guarantees, either expressed or implied, including but
 * not limited to fitness for a particular purpose.
 * 
 * Contributions:
 * Contributions to this project are welcome. By submitting 
 * code, you agree to license your contributions under the 
 * same BSD-3-Clause terms.
 * 
 * See LICENSE file for full details.
 ************************************************/

#include <QString>

// Gruvbox Color Palette
namespace Gruvbox
{
constexpr auto Background    = "#282828";
constexpr auto Foreground    = "#ebdbb2";
constexpr auto Button        = "#3c3836";
constexpr auto ButtonText    = "#ebdbb2";
constexpr auto Highlight     = "#458588";
constexpr auto HighlightText = "#282828";
} // namespace Gruvbox

// Stylesheets
namespace Styles
{
const QString WindowPalette =
  QString("background-color: %1; color: %2;").arg(Gruvbox::Background, Gruvbox::Foreground);

const QString ButtonStyle =
  QString("background-color: %1; color: %2; padding: 8px; font-size: 14px; border-radius: 5px;")
    .arg(Gruvbox::Button, Gruvbox::ButtonText);

const QString LabelStyle = QString("color: %1; font-size: 12px;").arg(Gruvbox::Foreground);

const QString TitleLabelStyle =
  QString("color: %1; font-size: 18px; font-weight: bold; margin-bottom: 10px;")
    .arg(Gruvbox::Foreground);

const QString FileDialogStyle =
  QString("background-color: %1; color: %2; selection-background-color: %3; selection-color: %4;")
    .arg(Gruvbox::Background, Gruvbox::Foreground, Gruvbox::Highlight, Gruvbox::HighlightText);
} // namespace Styles
