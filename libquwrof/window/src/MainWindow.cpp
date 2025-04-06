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

#include <QFileDialog>
#include <libquwrof/schema/schema.hpp>
#include <libquwrof/window/entry.hpp>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
  setWindowTitle("Wavy");
  resize(400, 200);

  // Apply Gruvbox Palette
  QPalette palette;
  palette.setColor(QPalette::Window, QColor(Gruvbox::Background));
  palette.setColor(QPalette::WindowText, QColor(Gruvbox::Foreground));
  palette.setColor(QPalette::Button, QColor(Gruvbox::Button));
  palette.setColor(QPalette::ButtonText, QColor(Gruvbox::ButtonText));
  palette.setColor(QPalette::Highlight, QColor(Gruvbox::Highlight));
  palette.setColor(QPalette::HighlightedText, QColor(Gruvbox::HighlightText));
  setPalette(palette);

  centralWidget = new QWidget(this);
  auto* layout  = new QVBoxLayout(centralWidget);

  titleLabel = new QLabel("Wavy", this);
  titleLabel->setAlignment(Qt::AlignTop);
  titleLabel->setStyleSheet(Styles::TitleLabelStyle);

  // Description Label
  descriptionLabel = new QLabel("Wavy is a lightweight audio streaming and sharing tool.", this);
  descriptionLabel->setAlignment(Qt::AlignCenter);
  descriptionLabel->setStyleSheet(Styles::LabelStyle);

  filePickerButton = new QPushButton("Pick a File", this);
  filePickerButton->setStyleSheet(Styles::ButtonStyle);

  filePathLabel = new QLabel("No file selected", this);
  filePathLabel->setStyleSheet(Styles::LabelStyle);

  connect(filePickerButton, &QPushButton::clicked, this, &MainWindow::openFileDialog);

  layout->addWidget(titleLabel, 0, Qt::AlignTop | Qt::AlignHCenter);
  layout->addWidget(descriptionLabel, 0, Qt::AlignTop | Qt::AlignHCenter);
  layout->addStretch(); // Push content down
  layout->addWidget(filePickerButton);
  layout->addWidget(filePathLabel);
  layout->addStretch(); // Balance bottom spacing
  layout->setAlignment(Qt::AlignCenter);

  centralWidget->setLayout(layout);
  setCentralWidget(centralWidget);
}

void MainWindow::openFileDialog()
{
  QFileDialog dialog(this);
  dialog.setWindowTitle("Select an Audio File");
  dialog.setNameFilter("Audio Files (*.flac *.mp3);;All Files (*)");
  dialog.setStyleSheet(Styles::FileDialogStyle);

  if (dialog.exec())
  {
    QString filePath = dialog.selectedFiles().first();
    if (!filePath.isEmpty() && (filePath.endsWith(".flac") || filePath.endsWith(".mp3")))
    {
      filePathLabel->setText("Selected: " + filePath);
    }
    else
    {
      filePathLabel->setText("Invalid file type or no file selected");
    }
  }
}

MainWindow::~MainWindow() = default;
