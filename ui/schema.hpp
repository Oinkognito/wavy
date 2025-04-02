#pragma once

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
